/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *  DOC: wlan_mgmt_txrx_rx_reo_i.h
 *  This file contains mgmt rx re-ordering related APIs
 */

#ifndef _WLAN_MGMT_TXRX_RX_REO_I_H
#define _WLAN_MGMT_TXRX_RX_REO_I_H

#ifdef WLAN_MGMT_RX_REO_SUPPORT
#include <qdf_list.h>
#include <qdf_timer.h>
#include <qdf_lock.h>
#include <qdf_nbuf.h>
#include <qdf_threads.h>
#include <qdf_defer.h>
#include <wlan_mgmt_txrx_rx_reo_utils_api.h>
#include <wlan_mgmt_txrx_rx_reo_public_structs.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_mlo_mgr_public_structs.h>

#define MGMT_RX_REO_INGRESS_LIST_MAX_SIZE                  (512)
#define MGMT_RX_REO_INGRESS_LIST_TIMEOUT_US                (250 * USEC_PER_MSEC)
#define MGMT_RX_REO_INGRESS_LIST_AGEOUT_TIMER_PERIOD_MS    (50)

#define MGMT_RX_REO_EGRESS_LIST_MAX_SIZE                   (256)

#define INGRESS_TO_EGRESS_MOVEMENT_TEMP_LIST_MAX_SIZE \
				(MGMT_RX_REO_INGRESS_LIST_MAX_SIZE)

#define MGMT_RX_REO_EGRESS_INACTIVITY_TIMEOUT    (10 * 60 * MSEC_PER_SEC)

#define STATUS_WAIT_FOR_FRAME_ON_OTHER_LINKS         (BIT(0))
#define STATUS_AGED_OUT                              (BIT(1))
#define STATUS_OLDER_THAN_LATEST_AGED_OUT_FRAME      (BIT(2))
#define STATUS_INGRESS_LIST_OVERFLOW                 (BIT(3))
#define STATUS_OLDER_THAN_READY_TO_DELIVER_FRAMES    (BIT(4))
#define STATUS_EGRESS_LIST_OVERFLOW                  (BIT(5))

#define MGMT_RX_REO_INVALID_LINK   (-1)

/* Reason to release an entry from the reorder list */
#define RELEASE_REASON_ZERO_WAIT_COUNT                          (BIT(0))
#define RELEASE_REASON_AGED_OUT                                 (BIT(1))
#define RELEASE_REASON_OLDER_THAN_AGED_OUT_FRAME                (BIT(2))
#define RELEASE_REASON_INGRESS_LIST_OVERFLOW                    (BIT(3))
#define RELEASE_REASON_OLDER_THAN_READY_TO_DELIVER_FRAMES       (BIT(4))
#define RELEASE_REASON_EGRESS_LIST_OVERFLOW                     (BIT(5))
#define RELEASE_REASON_MAX  \
		(RELEASE_REASON_EGRESS_LIST_OVERFLOW << 1)

#define LIST_ENTRY_IS_WAITING_FOR_FRAME_ON_OTHER_LINK(entry)   \
	((entry)->status & STATUS_WAIT_FOR_FRAME_ON_OTHER_LINKS)
#define LIST_ENTRY_IS_AGED_OUT(entry)   \
	((entry)->status & STATUS_AGED_OUT)
#define LIST_ENTRY_IS_OLDER_THAN_LATEST_AGED_OUT_FRAME(entry)  \
	((entry)->status & STATUS_OLDER_THAN_LATEST_AGED_OUT_FRAME)
#define LIST_ENTRY_IS_REMOVED_DUE_TO_INGRESS_LIST_OVERFLOW(entry)  \
	((entry)->status & STATUS_INGRESS_LIST_OVERFLOW)
#define LIST_ENTRY_IS_OLDER_THAN_READY_TO_DELIVER_FRAMES(entry)  \
	((entry)->status & STATUS_OLDER_THAN_READY_TO_DELIVER_FRAMES)
#define LIST_ENTRY_IS_REMOVED_DUE_TO_EGRESS_LIST_OVERFLOW(entry)  \
	((entry)->status & STATUS_EGRESS_LIST_OVERFLOW)

#ifdef WLAN_MGMT_RX_REO_DEBUG_SUPPORT
#define MGMT_RX_REO_EGRESS_FRAME_DEBUG_INFO_BOARDER_MAX_SIZE   (848)
#define MGMT_RX_REO_EGRESS_FRAME_DELIVERY_REASON_STATS_BOARDER_A_MAX_SIZE  (66)
#define MGMT_RX_REO_EGRESS_FRAME_DELIVERY_REASON_STATS_BOARDER_B_MAX_SIZE  (73)
#define MGMT_RX_REO_EGRESS_FRAME_DEBUG_INFO_FLAG_MAX_SIZE   (3)
#define MGMT_RX_REO_EGRESS_FRAME_DEBUG_INFO_WAIT_COUNT_MAX_SIZE   (69)
#define MGMT_RX_REO_EGRESS_FRAME_DEBUG_INFO_PER_LINK_SNAPSHOTS_MAX_SIZE   (94)
#define MGMT_RX_REO_EGRESS_FRAME_DEBUG_INFO_SNAPSHOT_MAX_SIZE     (22)
#define MGMT_RX_REO_EGRESS_FRAME_DEBUG_INFO_PRINT_MAX_FRAMES     (0)

#define MGMT_RX_REO_INGRESS_FRAME_DEBUG_INFO_BOARDER_MAX_SIZE   (807)
#define MGMT_RX_REO_INGRESS_FRAME_DEBUG_INFO_FLAG_MAX_SIZE   (13)
#define MGMT_RX_REO_INGRESS_FRAME_DEBUG_INFO_WAIT_COUNT_MAX_SIZE   (69)
#define MGMT_RX_REO_INGRESS_FRAME_DEBUG_INFO_PER_LINK_SNAPSHOTS_MAX_SIZE   (94)
#define MGMT_RX_REO_INGRESS_FRAME_DEBUG_INFO_SNAPSHOT_MAX_SIZE     (22)
#define MGMT_RX_REO_INGRESS_FRAME_DEBUG_INFO_PRINT_MAX_FRAMES     (0)
#endif /* WLAN_MGMT_RX_REO_DEBUG_SUPPORT*/

/*
 * struct mgmt_rx_reo_pdev_info - Pdev information required by the Management
 * Rx REO module
 * @host_snapshot: Latest snapshot seen at the Host.
 * It considers both MGMT Rx and MGMT FW consumed.
 * @last_valid_shared_snapshot: Array of last valid snapshots(for snapshots
 * shared between host and target)
 * @host_target_shared_snapshot_info: Array of meta information related to
 * snapshots(for snapshots shared between host and target)
 * @filter: MGMT Rx REO filter
 * @init_complete: Flag to indicate initialization completion of the
 * mgmt_rx_reo_pdev_info object
 */
struct mgmt_rx_reo_pdev_info {
	struct mgmt_rx_reo_snapshot_params host_snapshot;
	struct mgmt_rx_reo_snapshot_params last_valid_shared_snapshot
				[MGMT_RX_REO_SHARED_SNAPSHOT_MAX];
	struct mgmt_rx_reo_snapshot_info host_target_shared_snapshot_info
				[MGMT_RX_REO_SHARED_SNAPSHOT_MAX];
	struct mgmt_rx_reo_filter filter;
	struct mgmt_rx_reo_shared_snapshot raw_snapshots[MAX_MLO_LINKS]
			[MGMT_RX_REO_SHARED_SNAPSHOT_MAX]
			[MGMT_RX_REO_SNAPSHOT_READ_RETRY_LIMIT]
			[MGMT_RX_REO_SNAPSHOT_B2B_READ_SWAR_RETRY_LIMIT];
	bool init_complete;
};

/**
 * mgmt_rx_reo_pdev_attach() - Initializes the per pdev data structures related
 * to management rx-reorder module
 * @pdev: pointer to pdev object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mgmt_rx_reo_pdev_attach(struct wlan_objmgr_pdev *pdev);

/**
 * mgmt_rx_reo_psoc_attach() - Initializes the per psoc data structures related
 * to management rx-reorder module
 * @psoc: pointer to psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mgmt_rx_reo_psoc_attach(struct wlan_objmgr_psoc *psoc);

/**
 * mgmt_rx_reo_pdev_detach() - Clears the per pdev data structures related to
 * management rx-reorder module
 * @pdev: pointer to pdev object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mgmt_rx_reo_pdev_detach(struct wlan_objmgr_pdev *pdev);

/**
 * mgmt_rx_reo_psoc_detach() - Clears the per psoc data structures related to
 * management rx-reorder module
 * @psoc: pointer to psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mgmt_rx_reo_psoc_detach(struct wlan_objmgr_psoc *psoc);

/**
 * mgmt_rx_reo_pdev_obj_create_notification() - pdev create handler for
 * management rx-reorder module
 * @pdev: pointer to pdev object
 * @mgmt_txrx_pdev_ctx: pdev private object of mgmt txrx module
 *
 * This function gets called from object manager when pdev is being created and
 * creates management rx-reorder pdev context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_pdev_obj_create_notification(
	struct wlan_objmgr_pdev *pdev,
	struct mgmt_txrx_priv_pdev_context *mgmt_txrx_pdev_ctx);

/**
 * mgmt_rx_reo_pdev_obj_destroy_notification() - pdev destroy handler for
 * management rx-reorder feature
 * @pdev: pointer to pdev object
 * @mgmt_txrx_pdev_ctx: pdev private object of mgmt txrx module
 *
 * This function gets called from object manager when pdev is being destroyed
 * and destroys management rx-reorder pdev context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_pdev_obj_destroy_notification(
	struct wlan_objmgr_pdev *pdev,
	struct mgmt_txrx_priv_pdev_context *mgmt_txrx_pdev_ctx);

/**
 * mgmt_rx_reo_psoc_obj_create_notification() - psoc create handler for
 * management rx-reorder module
 * @psoc: pointer to psoc object
 *
 * This function gets called from object manager when psoc is being created.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc);

/**
 * mgmt_rx_reo_psoc_obj_destroy_notification() - psoc destroy handler for
 * management rx-reorder feature
 * @psoc: pointer to psoc object
 *
 * This function gets called from object manager when psoc is being destroyed.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc);

/**
 * enum mgmt_rx_reo_frame_descriptor_type - Enumeration for management frame
 * descriptor type.
 * @MGMT_RX_REO_FRAME_DESC_HOST_CONSUMED_FRAME: Management frame to be consumed
 * by host.
 * @MGMT_RX_REO_FRAME_DESC_FW_CONSUMED_FRAME: Management frame consumed by FW
 * @MGMT_RX_REO_FRAME_DESC_ERROR_FRAME: Management frame which got dropped
 * at host due to any error
 * @MGMT_RX_REO_FRAME_DESC_TYPE_MAX: Maximum number of frame types
 */
enum mgmt_rx_reo_frame_descriptor_type {
	MGMT_RX_REO_FRAME_DESC_HOST_CONSUMED_FRAME = 0,
	MGMT_RX_REO_FRAME_DESC_FW_CONSUMED_FRAME,
	MGMT_RX_REO_FRAME_DESC_ERROR_FRAME,
	MGMT_RX_REO_FRAME_DESC_TYPE_MAX,
};

/**
 * enum mgmt_rx_reo_list_type - Enumeration for management rx reorder list type
 * @MGMT_RX_REO_LIST_TYPE_INGRESS: Ingress list type
 * @MGMT_RX_REO_LIST_TYPE_EGRESS: Egress list type
 * @MGMT_RX_REO_LIST_TYPE_MAX: Maximum number of list types
 * @MGMT_RX_REO_LIST_TYPE_INVALID: Invalid list type
 */
enum mgmt_rx_reo_list_type {
	MGMT_RX_REO_LIST_TYPE_INGRESS = 0,
	MGMT_RX_REO_LIST_TYPE_EGRESS,
	MGMT_RX_REO_LIST_TYPE_MAX,
	MGMT_RX_REO_LIST_TYPE_INVALID,
};

/**
 * enum mgmt_rx_reo_ingress_drop_reason - Enumeration for management rx reorder
 * reason code for dropping an incoming management frame
 * @MGMT_RX_REO_INGRESS_DROP_REASON_INVALID: Invalid ingress drop reason code
 * @MGMT_RX_REO_INVALID_REO_PARAMS: Invalid reo parameters
 * @MGMT_RX_REO_OUT_OF_ORDER_PKT_CTR: Packet counter of the current frame is
 * less than the packet counter of the last frame in the same link
 * @MGMT_RX_REO_DUPLICATE_PKT_CTR: Packet counter of the current frame is same
 * as the packet counter of the last frame
 * @MGMT_RX_REO_ZERO_DURATION: Zero duration for a management frame which is
 * supposed to be consumed by host
 * @MGMT_RX_REO_SNAPSHOT_SANITY_FAILURE: Snapshot sanity failure in any of the
 * links
 * @MGMT_RX_REO_INGRESS_DROP_REASON_MAX: Maximum value of ingress drop reason
 * code
 */
enum mgmt_rx_reo_ingress_drop_reason {
	MGMT_RX_REO_INGRESS_DROP_REASON_INVALID = 0,
	MGMT_RX_REO_INVALID_REO_PARAMS,
	MGMT_RX_REO_OUT_OF_ORDER_PKT_CTR,
	MGMT_RX_REO_DUPLICATE_PKT_CTR,
	MGMT_RX_REO_ZERO_DURATION,
	MGMT_RX_REO_SNAPSHOT_SANITY_FAILURE,
	MGMT_RX_REO_INGRESS_DROP_REASON_MAX,
};

/**
 * enum mgmt_rx_reo_execution_context - Execution contexts related to management
 * Rx reorder
 * @MGMT_RX_REO_CONTEXT_MGMT_RX: Incoming mgmt Rx context
 * @MGMT_RX_REO_CONTEXT_INGRESS_LIST_TIMEOUT: Ingress list time out context
 * @MGMT_RX_REO_CONTEXT_SCHEDULER_CB: Schgeduler call back context
 * @MGMT_RX_REO_CONTEXT_MAX: Maximum number of execution contexts
 * @MGMT_RX_REO_CONTEXT_INVALID: Invalid execution context
 */
enum mgmt_rx_reo_execution_context {
	MGMT_RX_REO_CONTEXT_MGMT_RX,
	MGMT_RX_REO_CONTEXT_INGRESS_LIST_TIMEOUT,
	MGMT_RX_REO_CONTEXT_SCHEDULER_CB,
	MGMT_RX_REO_CONTEXT_MAX,
	MGMT_RX_REO_CONTEXT_INVALID,
};

/**
 * struct mgmt_rx_reo_context_info - This structure holds the information
 * about the current execution context
 * @context: Current execution context
 * @in_reo_params: Reo parameters of the current management frame
 * @context_id: Context identifier
 */
struct mgmt_rx_reo_context_info {
	enum mgmt_rx_reo_execution_context context;
	struct mgmt_rx_reo_params in_reo_params;
	int32_t context_id;
};

/**
 * struct mgmt_rx_reo_frame_info - This structure holds the information
 * about a management frame.
 * @valid: Indicates whether the structure content is valid
 * @reo_params: Management Rx reorder parameters
 */
struct mgmt_rx_reo_frame_info {
	bool valid;
	struct mgmt_rx_reo_params reo_params;
};

/**
 * struct mgmt_rx_reo_list - Linked list used to reorder/deliver the management
 * frames received. Each list entry would correspond to a management frame. List
 * entries would be sorted in the same order in which they are received by
 * MAC HW.
 * @list: Linked list
 * @list_lock: Spin lock to protect the list
 * @max_list_size: Maximum size of the reorder list
 * @overflow_count: Number of times list overflow occurred
 * @last_overflow_ts: Host time stamp of last overflow
 * @last_inserted_frame: Information about the last frame inserted to the list
 * @last_released_frame: Information about the last frame released from the list
 */
struct mgmt_rx_reo_list {
	qdf_list_t list;
	qdf_spinlock_t list_lock;
	uint32_t max_list_size;
	uint64_t overflow_count;
	uint64_t last_overflow_ts;
	struct mgmt_rx_reo_frame_info last_inserted_frame;
	struct mgmt_rx_reo_frame_info last_released_frame;
};

/**
 * struct mgmt_rx_reo_ingress_list - Linked list used to reorder the management
 * frames received
 * @reo_list: Linked list used for reordering
 * @list_entry_timeout_us: Time out value(microsecond) for the list entries
 * @ageout_timer: Periodic timer to age-out the list entries
 */
struct mgmt_rx_reo_ingress_list {
	struct mgmt_rx_reo_list reo_list;
	uint32_t list_entry_timeout_us;
	qdf_timer_t ageout_timer;
};

/**
 * struct mgmt_rx_reo_egress_list - Linked list used to store the frames which
 * are reordered and pending delivery
 * @reo_list: Linked list used for storing the frames which are reordered and
 * pending delivery
 * @egress_inactivity_timer: Management Rx inactivity timer
 */
struct mgmt_rx_reo_egress_list {
	struct mgmt_rx_reo_list reo_list;
	qdf_timer_t egress_inactivity_timer;
};

/*
 * struct mgmt_rx_reo_wait_count - Wait count for a mgmt frame
 * @per_link_count: Array of wait counts for all MLO links. Each array entry
 * holds the number of frames this mgmt frame should wait for on that
 * particular link.
 * @total_count: Sum of entries in @per_link_count
 */
struct mgmt_rx_reo_wait_count {
	unsigned int per_link_count[MAX_MLO_LINKS];
	unsigned long long int total_count;
};

/**
 * struct mgmt_rx_reo_list_entry - Entry in the Management reorder list
 * @node: List node
 * @nbuf: nbuf corresponding to this frame
 * @rx_params: Management rx event parameters
 * @wait_count: Wait counts for the frame
 * @initial_wait_count: Wait count when the frame is queued
 * @ingress_timestamp: Host time stamp when this frame has arrived reorder
 * module
 * @ingress_list_insertion_ts: Host time stamp when this entry is inserted to
 * the ingress list.
 * @ingress_list_removal_ts: Host time stamp when this entry is removed from
 * the ingress list
 * @egress_list_insertion_ts: Host time stamp when this entry is inserted to
 * the egress list.
 * @egress_list_removal_ts: Host time stamp when this entry is removed from
 * the egress list
 * @first_scheduled_ts: Host time stamp when this entry is first scheduled
 * by scheduler
 * @last_scheduled_ts: Host time stamp when this entry is last scheduled
 * by scheduler
 * @egress_timestamp: Host time stamp when this frame has exited reorder
 * module
 * @egress_list_size: Egress list size just before removing this frame
 * @status: Status for this entry
 * @pdev: Pointer to pdev object corresponding to this frame
 * @release_reason: Release reason
 * @is_delivered: Indicates whether the frame is delivered successfully
 * @is_dropped: Indciates whether the frame is dropped in reo layer
 * @is_premature_delivery: Indicates whether the frame is delivered
 * prematurely
 * @is_parallel_rx: Indicates that this frame is received in parallel to the
 * last frame which is delivered to the upper layer.
 * @shared_snapshots: snapshots shared b/w host and target
 * @host_snapshot: host snapshot
 * @scheduled_count: Number of times scheduler is invoked for this frame
 * @ctx_info: Execution context info
 */
struct mgmt_rx_reo_list_entry {
	qdf_list_node_t node;
	qdf_nbuf_t nbuf;
	struct mgmt_rx_event_params *rx_params;
	struct mgmt_rx_reo_wait_count wait_count;
	struct mgmt_rx_reo_wait_count initial_wait_count;
	uint64_t ingress_timestamp;
	uint64_t ingress_list_insertion_ts;
	uint64_t ingress_list_removal_ts;
	uint64_t egress_list_insertion_ts;
	uint64_t egress_list_removal_ts;
	uint64_t first_scheduled_ts;
	uint64_t last_scheduled_ts;
	uint64_t egress_timestamp;
	uint64_t egress_list_size;
	uint32_t status;
	struct wlan_objmgr_pdev *pdev;
	uint8_t release_reason;
	bool is_delivered;
	bool is_dropped;
	bool is_premature_delivery;
	bool is_parallel_rx;
	struct mgmt_rx_reo_snapshot_params shared_snapshots
			[MAX_MLO_LINKS][MGMT_RX_REO_SHARED_SNAPSHOT_MAX];
	struct mgmt_rx_reo_snapshot_params host_snapshot[MAX_MLO_LINKS];
	qdf_atomic_t scheduled_count;
	struct mgmt_rx_reo_context_info ctx_info;
};

#ifdef WLAN_MGMT_RX_REO_SIM_SUPPORT

#define MGMT_RX_REO_SIM_INTER_FRAME_DELAY_MIN             (300 * USEC_PER_MSEC)
#define MGMT_RX_REO_SIM_INTER_FRAME_DELAY_MIN_MAX_DELTA   (200 * USEC_PER_MSEC)

#define MGMT_RX_REO_SIM_DELAY_MAC_HW_TO_FW_MIN            (1000 * USEC_PER_MSEC)
#define MGMT_RX_REO_SIM_DELAY_MAC_HW_TO_FW_MIN_MAX_DELTA  (500 * USEC_PER_MSEC)

#define MGMT_RX_REO_SIM_DELAY_FW_TO_HOST_MIN              (1000 * USEC_PER_MSEC)
#define MGMT_RX_REO_SIM_DELAY_FW_TO_HOST_MIN_MAX_DELTA    (500 * USEC_PER_MSEC)

#define MGMT_RX_REO_SIM_PERCENTAGE_FW_CONSUMED_FRAMES  (10)
#define MGMT_RX_REO_SIM_PERCENTAGE_ERROR_FRAMES        (10)

#define MGMT_RX_REO_SIM_PENDING_FRAME_LIST_MAX_SIZE    (1000)
#define MGMT_RX_REO_SIM_STALE_FRAME_LIST_MAX_SIZE      \
				(MGMT_RX_REO_SIM_PENDING_FRAME_LIST_MAX_SIZE)
#define MGMT_RX_REO_SIM_STALE_FRAME_TEMP_LIST_MAX_SIZE (100)

/**
 * struct mgmt_rx_frame_params - Parameters associated with a management frame.
 * This structure is used by the simulation framework.
 * @link_id: MLO HW link id
 * @mgmt_pkt_ctr: Management packet counter
 * @global_timestamp: Global time stamp in micro seconds
 */
struct mgmt_rx_frame_params {
	uint8_t link_id;
	uint16_t mgmt_pkt_ctr;
	uint32_t global_timestamp;
};

/**
 * struct mgmt_rx_reo_master_frame_list - List which contains all the
 * management frames received and not yet consumed by FW/Host. Order of frames
 * in the list is same as the order in which they are received in the air.
 * This is used by the simulation framework to confirm that the outcome of
 * reordering is correct.
 * @pending_list: List which contains all the frames received after the
 * last frame delivered to upper layer. These frames will eventually reach host.
 * @stale_list: List which contains all the stale management frames which
 * are not yet consumed by FW/Host. Stale management frames are the frames which
 * are older than last delivered frame to upper layer.
 * @lock: Spin lock to protect pending frame list and stale frame list.
 */
struct mgmt_rx_reo_master_frame_list {
	qdf_list_t pending_list;
	qdf_list_t stale_list;
	qdf_spinlock_t lock;
};

/**
 * struct mgmt_rx_reo_pending_frame_list_entry - Structure used to represent an
 * entry in the pending frame list.
 * @params: parameters related to the management frame
 * @node: linked list node
 */
struct mgmt_rx_reo_pending_frame_list_entry {
	struct mgmt_rx_frame_params params;
	qdf_list_node_t node;
};

/**
 * struct mgmt_rx_reo_stale_frame_list_entry - Structure used to represent an
 * entry in the stale frame list.
 * @params: parameters related to the management frame
 * @node: linked list node
 */
struct mgmt_rx_reo_stale_frame_list_entry {
	struct mgmt_rx_frame_params params;
	qdf_list_node_t node;
};

/**
 * struct mgmt_rx_frame_mac_hw - Structure used to represent the management
 * frame at MAC HW level
 * @params: parameters related to the management frame
 * @frame_handler_fw: Work structure to queue the frame to the FW
 * @sim_context: pointer management rx-reorder simulation context
 */
struct mgmt_rx_frame_mac_hw {
	struct mgmt_rx_frame_params params;
	qdf_work_t frame_handler_fw;
	struct mgmt_rx_reo_sim_context *sim_context;
};

/**
 * struct mgmt_rx_frame_fw - Structure used to represent the management
 * frame at FW level
 * @params: parameters related to the management frame
 * @is_consumed_by_fw: indicates whether the frame is consumed by FW
 * @frame_handler_host: Work structure to queue the frame to the host
 * @sim_context: pointer management rx-reorder simulation context
 */
struct mgmt_rx_frame_fw {
	struct mgmt_rx_frame_params params;
	bool is_consumed_by_fw;
	qdf_work_t frame_handler_host;
	struct mgmt_rx_reo_sim_context *sim_context;
};

/**
 * struct mgmt_rx_reo_sim_mac_hw - Structure used to represent the MAC HW
 * @mgmt_pkt_ctr: Stores the last management packet counter for all the links
 */
struct mgmt_rx_reo_sim_mac_hw {
	uint16_t mgmt_pkt_ctr[MAX_MLO_LINKS];
};

/**
 * struct mgmt_rx_reo_sim_link_id_to_pdev_map - Map from link id to pdev
 * object. This is used for simulation purpose only.
 * @map: link id to pdev map. Link id is the array index.
 * @lock: lock used to protect this structure
 * @num_mlo_links: Total number of MLO HW links. In case of simulation all the
 * pdevs are assumed to have MLO capability and number of MLO links is same as
 * the number of pdevs in the system.
 * @valid_link_list: List of valid link id values
 */
struct mgmt_rx_reo_sim_link_id_to_pdev_map {
	struct wlan_objmgr_pdev *map[MAX_MLO_LINKS];
	qdf_spinlock_t lock;
	uint8_t num_mlo_links;
	int8_t valid_link_list[MAX_MLO_LINKS];
};

/**
 * struct mgmt_rx_reo_mac_hw_simulator - Structure which stores the members
 * required for the MAC HW simulation
 * @mac_hw_info: MAC HW info
 * @mac_hw_thread: kthread which simulates MAC HW
 */
struct mgmt_rx_reo_mac_hw_simulator {
	struct mgmt_rx_reo_sim_mac_hw mac_hw_info;
	qdf_thread_t *mac_hw_thread;
};

/**
 * struct mgmt_rx_reo_sim_context - Management rx-reorder simulation context
 * @host_mgmt_frame_handler: Per link work queue to simulate the host layer
 * @fw_mgmt_frame_handler: Per link work queue to simulate the FW layer
 * @master_frame_list: List used to store information about all the management
 * frames
 * @mac_hw_sim:  MAC HW simulation object
 * @snapshot: snapshots required for reo algorithm
 * @link_id_to_pdev_map: link_id to pdev object map
 * @mlo_grp_id: MLO group id which it belongs to
 */
struct mgmt_rx_reo_sim_context {
	struct workqueue_struct *host_mgmt_frame_handler[MAX_MLO_LINKS];
	struct workqueue_struct *fw_mgmt_frame_handler[MAX_MLO_LINKS];
	struct mgmt_rx_reo_master_frame_list master_frame_list;
	struct mgmt_rx_reo_mac_hw_simulator mac_hw_sim;
	struct mgmt_rx_reo_shared_snapshot snapshot[MAX_MLO_LINKS]
					    [MGMT_RX_REO_SHARED_SNAPSHOT_MAX];
	struct mgmt_rx_reo_sim_link_id_to_pdev_map link_id_to_pdev_map;
	uint8_t mlo_grp_id;
};
#endif /* WLAN_MGMT_RX_REO_SIM_SUPPORT */

#ifdef WLAN_MGMT_RX_REO_DEBUG_SUPPORT
/**
 * struct reo_ingress_debug_frame_info - Debug information about a frame
 * entering reorder algorithm
 * @link_id: link id
 * @mgmt_pkt_ctr: management packet counter
 * @global_timestamp: MLO global time stamp
 * @start_timestamp: start time stamp of the frame
 * @end_timestamp: end time stamp of the frame
 * @duration_us: duration of the frame in us
 * @desc_type: Type of the frame descriptor
 * @frame_type: frame type
 * @frame_subtype: frame sub type
 * @ingress_timestamp: Host time stamp when the frames enters the reorder
 * algorithm
 * @ingress_duration: Duration in us for processing the incoming frame.
 * ingress_duration = Time stamp at which reorder list update is done -
 * Time stamp at which frame has entered the reorder module
 * @wait_count: Wait count calculated for the current frame
 * @is_queued: Indicates whether this frame is queued to reorder list
 * @is_stale: Indicates whether this frame is stale.
 * @is_parallel_rx: Indicates that this frame is received in parallel to the
 * last frame which is delivered to the upper layer.
 * @zero_wait_count_rx: Indicates whether this frame's wait count was
 * zero when received by host
 * @immediate_delivery: Indicates whether this frame can be delivered
 * immediately to the upper layers
 * @queued_list: Type of list in which the current frame is queued
 * @is_error: Indicates whether any error occurred during processing this frame
 * @last_delivered_frame: Stores the information about the last frame delivered
 * to the upper layer
 * @ingress_list_size_rx: Size of the ingress list when this frame is
 * received (before updating the ingress list based on this frame).
 * @ingress_list_insertion_pos: Position in the ingress list where this
 * frame is going to get inserted (Applicable for only host consumed frames)
 * @egress_list_size_rx: Size of the egress list when this frame is
 * added to the egress list
 * @egress_list_insertion_pos: Position in the egress list where this
 * frame is going to get inserted
 * @shared_snapshots: snapshots shared b/w host and target
 * @host_snapshot: host snapshot
 * @cpu_id: CPU index
 * @reo_required: Indicates whether reorder is required for the current frame.
 * If reorder is not required, current frame will just be used for updating the
 * wait count of frames already part of the reorder list.
 * @context_id: Context identifier
 * @drop_reason: Reason for dropping the frame
 * @drop: Indicates whether the frame has to be dropped
 */
struct reo_ingress_debug_frame_info {
	uint8_t link_id;
	uint16_t mgmt_pkt_ctr;
	uint32_t global_timestamp;
	uint32_t start_timestamp;
	uint32_t end_timestamp;
	uint32_t duration_us;
	enum mgmt_rx_reo_frame_descriptor_type desc_type;
	uint8_t frame_type;
	uint8_t frame_subtype;
	uint64_t ingress_timestamp;
	uint64_t ingress_duration;
	struct mgmt_rx_reo_wait_count wait_count;
	bool is_queued;
	bool is_stale;
	bool is_parallel_rx;
	bool zero_wait_count_rx;
	bool immediate_delivery;
	enum mgmt_rx_reo_list_type queued_list;
	bool is_error;
	struct mgmt_rx_reo_frame_info last_delivered_frame;
	int16_t ingress_list_size_rx;
	int16_t ingress_list_insertion_pos;
	int16_t egress_list_size_rx;
	int16_t egress_list_insertion_pos;
	struct mgmt_rx_reo_snapshot_params shared_snapshots
			[MAX_MLO_LINKS][MGMT_RX_REO_SHARED_SNAPSHOT_MAX];
	struct mgmt_rx_reo_snapshot_params host_snapshot[MAX_MLO_LINKS];
	int cpu_id;
	bool reo_required;
	int32_t context_id;
	enum mgmt_rx_reo_ingress_drop_reason drop_reason;
	bool drop;
};

/**
 * struct reo_egress_debug_frame_info - Debug information about a frame
 * leaving the reorder module
 * @is_delivered: Indicates whether the frame is delivered to upper layers
 * @is_dropped: Indciates whether the frame is dropped in reo layer
 * @is_premature_delivery: Indicates whether the frame is delivered
 * prematurely
 * @link_id: link id
 * @mgmt_pkt_ctr: management packet counter
 * @global_timestamp: MLO global time stamp
 * @ingress_timestamp: Host time stamp when the frame enters the reorder module
 * @ingress_list_insertion_ts: Host time stamp when this entry is inserted to
 * the ingress list.
 * @ingress_list_removal_ts: Host time stamp when this entry is removed from
 * the ingress list
 * @egress_list_insertion_ts: Host time stamp when this entry is inserted to
 * the egress list.
 * @egress_list_removal_ts: Host time stamp when this entry is removed from
 * the egress list
 * @egress_timestamp: Host time stamp just before delivery of the frame to upper
 * layer
 * @egress_duration: Duration in us taken by the upper layer to process
 * the frame.
 * @egress_list_size: Egress list size just before removing this frame
 * @first_scheduled_ts: Host time stamp when this entry is first scheduled for
 * delivery
 * @last_scheduled_ts: Host time stamp when this entry is last scheduled for
 * delivery
 * @scheduled_count: Number of times this entry is scheduled
 * @initial_wait_count: Wait count when the frame is queued
 * @final_wait_count: Wait count when frame is released to upper layer
 * @release_reason: Reason for delivering the frame to upper layers
 * @shared_snapshots: snapshots shared b/w host and target
 * @host_snapshot: host snapshot
 * @cpu_id: CPU index
 * @ctx_info: Execution context info
 */
struct reo_egress_debug_frame_info {
	bool is_delivered;
	bool is_dropped;
	bool is_premature_delivery;
	uint8_t link_id;
	uint16_t mgmt_pkt_ctr;
	uint32_t global_timestamp;
	uint64_t ingress_timestamp;
	uint64_t ingress_list_insertion_ts;
	uint64_t ingress_list_removal_ts;
	uint64_t egress_list_insertion_ts;
	uint64_t egress_list_removal_ts;
	uint64_t egress_timestamp;
	uint64_t egress_duration;
	uint64_t egress_list_size;
	uint64_t first_scheduled_ts;
	uint64_t last_scheduled_ts;
	int32_t scheduled_count;
	struct mgmt_rx_reo_wait_count initial_wait_count;
	struct mgmt_rx_reo_wait_count final_wait_count;
	uint8_t release_reason;
	struct mgmt_rx_reo_snapshot_params shared_snapshots
			[MAX_MLO_LINKS][MGMT_RX_REO_SHARED_SNAPSHOT_MAX];
	struct mgmt_rx_reo_snapshot_params host_snapshot[MAX_MLO_LINKS];
	int cpu_id;
	struct mgmt_rx_reo_context_info ctx_info;
};

/**
 * struct reo_ingress_frame_stats - Structure to store statistics related to
 * incoming frames
 * @ingress_count: Number of frames entering reo module
 * @reo_count: Number of frames for which reorder is required
 * @queued_count: Number of frames queued to reorder list
 * @zero_wait_count_rx_count: Number of frames for which wait count is
 * zero when received at host
 * @immediate_delivery_count: Number of frames which can be delivered
 * immediately to the upper layers without reordering. A frame can be
 * immediately delivered if it has wait count of zero on reception at host
 * and the global time stamp is less than or equal to the global time
 * stamp of all the frames in the reorder list. Such frames would get
 * inserted to the head of the reorder list and gets delivered immediately
 * to the upper layers.
 * @stale_count: Number of stale frames. Any frame older than the
 * last frame delivered to upper layer is a stale frame.
 * @error_count: Number of frames dropped due to error occurred
 * within the reorder module
 * @parallel_rx_count: Number of frames which are categorised as parallel rx
 * @missing_count: Number of frames missing. This is calculated based on the
 * packet counter holes.
 * @drop_count: Number of frames dropped by host.
 */
struct reo_ingress_frame_stats {
	uint64_t ingress_count
		[MAX_MLO_LINKS][MGMT_RX_REO_FRAME_DESC_TYPE_MAX];
	uint64_t reo_count
		[MAX_MLO_LINKS][MGMT_RX_REO_FRAME_DESC_TYPE_MAX];
	uint64_t queued_count[MAX_MLO_LINKS][MGMT_RX_REO_LIST_TYPE_MAX];
	uint64_t zero_wait_count_rx_count
		[MAX_MLO_LINKS][MGMT_RX_REO_LIST_TYPE_MAX];
	uint64_t immediate_delivery_count
		[MAX_MLO_LINKS][MGMT_RX_REO_LIST_TYPE_MAX];
	uint64_t stale_count[MAX_MLO_LINKS]
			    [MGMT_RX_REO_FRAME_DESC_TYPE_MAX];
	uint64_t error_count[MAX_MLO_LINKS]
			    [MGMT_RX_REO_FRAME_DESC_TYPE_MAX];
	uint64_t parallel_rx_count[MAX_MLO_LINKS]
			    [MGMT_RX_REO_FRAME_DESC_TYPE_MAX];
	uint64_t missing_count[MAX_MLO_LINKS];
	uint64_t drop_count[MAX_MLO_LINKS][MGMT_RX_REO_INGRESS_DROP_REASON_MAX];
};

/**
 * struct reo_egress_frame_stats - Structure to store statistics related to
 * outgoing frames
 * @delivery_attempts_count: Number of attempts to deliver management
 * frames to upper layers
 * @delivery_success_count: Number of successful management frame
 * deliveries to upper layer
 * @drop_count: Number of management frames dropped within reo layer
 * @premature_delivery_count:  Number of frames delivered
 * prematurely. Premature delivery is the delivery of a management frame
 * to the upper layers even before its wait count is reaching zero.
 * @delivery_reason_count: Number frames delivered successfully for
 * each link and release reason.
 * @delivery_context_count: Number frames delivered successfully for
 * each link and execution context.
 */
struct reo_egress_frame_stats {
	uint64_t delivery_attempts_count[MAX_MLO_LINKS];
	uint64_t delivery_success_count[MAX_MLO_LINKS];
	uint64_t drop_count[MAX_MLO_LINKS];
	uint64_t premature_delivery_count[MAX_MLO_LINKS];
	uint64_t delivery_reason_count[MAX_MLO_LINKS][RELEASE_REASON_MAX];
	uint64_t delivery_context_count[MAX_MLO_LINKS][MGMT_RX_REO_CONTEXT_MAX];
};

/**
 * struct reo_ingress_debug_info - Circular array to store the
 * debug information about the frames entering the reorder algorithm.
 * @frame_list: Circular array to store the debug info about frames
 * @frame_list_size: Size of circular array @frame_list
 * @next_index: The index at which information about next frame will be logged
 * @wrap_aroud: Flag to indicate whether wrap around occurred when logging
 * debug information to @frame_list
 * @stats: Stats related to incoming frames
 * @boarder: boarder string
 */
struct reo_ingress_debug_info {
	struct reo_ingress_debug_frame_info *frame_list;
	uint16_t frame_list_size;
	int next_index;
	bool wrap_aroud;
	struct reo_ingress_frame_stats stats;
	char boarder[MGMT_RX_REO_INGRESS_FRAME_DEBUG_INFO_BOARDER_MAX_SIZE + 1];
};

/**
 * struct reo_egress_debug_info - Circular array to store the
 * debug information about the frames leaving the reorder module.
 * @frame_list: Circular array to store the debug info
 * @frame_list_size: Size of circular array @frame_list
 * @next_index: The index at which information about next frame will be logged
 * @wrap_aroud: Flag to indicate whether wrap around occurred when logging
 * debug information to @frame_list
 * @stats: Stats related to outgoing frames
 * @boarder: boarder string
 */
struct reo_egress_debug_info {
	struct reo_egress_debug_frame_info *frame_list;
	uint16_t frame_list_size;
	int next_index;
	bool wrap_aroud;
	struct reo_egress_frame_stats stats;
	char boarder[MGMT_RX_REO_EGRESS_FRAME_DEBUG_INFO_BOARDER_MAX_SIZE + 1];
};

/**
 * struct reo_scheduler_debug_frame_info - Debug information about a frame
 * gettign scheduled by management Rx reo scheduler
 * @link_id: link id
 * @mgmt_pkt_ctr: management packet counter
 * @global_timestamp: MLO global time stamp
 * @ingress_timestamp: Host time stamp when the frame enters the reorder module
 * @ingress_list_insertion_ts: Host time stamp when this entry is inserted to
 * the ingress list.
 * @ingress_list_removal_ts: Host time stamp when this entry is removed from
 * the ingress list
 * @egress_list_insertion_ts: Host time stamp when this entry is inserted to
 * the egress list.
 * @scheduled_ts: Host time stamp when this entry is scheduled for delivery
 * @first_scheduled_ts: Host time stamp when this entry is first scheduled for
 * delivery
 * @last_scheduled_ts: Host time stamp when this entry is last scheduled for
 * delivery
 * @scheduled_count: Number of times this entry is scheduled
 * @initial_wait_count: Wait count when the frame is queued
 * @final_wait_count: Wait count when frame is released to upper layer
 * @shared_snapshots: snapshots shared b/w host and target
 * @host_snapshot: host snapshot
 * @cpu_id: CPU index
 * @ctx_info: Execution context info
 */
struct reo_scheduler_debug_frame_info {
	uint8_t link_id;
	uint16_t mgmt_pkt_ctr;
	uint32_t global_timestamp;
	uint64_t ingress_timestamp;
	uint64_t ingress_list_insertion_ts;
	uint64_t ingress_list_removal_ts;
	uint64_t egress_list_insertion_ts;
	uint64_t scheduled_ts;
	uint64_t first_scheduled_ts;
	uint64_t last_scheduled_ts;
	int32_t scheduled_count;
	struct mgmt_rx_reo_wait_count initial_wait_count;
	struct mgmt_rx_reo_wait_count final_wait_count;
	struct mgmt_rx_reo_snapshot_params shared_snapshots
			[MAX_MLO_LINKS][MGMT_RX_REO_SHARED_SNAPSHOT_MAX];
	struct mgmt_rx_reo_snapshot_params host_snapshot[MAX_MLO_LINKS];
	int cpu_id;
	struct mgmt_rx_reo_context_info ctx_info;
};

/**
 * struct reo_scheduler_stats - Structure to store statistics related to
 * frames scheduled by reo scheduler
 * @scheduled_count: Scheduled count
 * @rescheduled_count: Rescheduled count
 * @scheduler_cb_count: Scheduler callback count
 */
struct reo_scheduler_stats {
	uint64_t scheduled_count[MAX_MLO_LINKS][MGMT_RX_REO_CONTEXT_MAX];
	uint64_t rescheduled_count[MAX_MLO_LINKS][MGMT_RX_REO_CONTEXT_MAX];
	uint64_t scheduler_cb_count[MAX_MLO_LINKS];
};

/**
 * struct reo_scheduler_debug_info - Circular array to store the
 * debug information about the frames scheduled by reo scheduler
 * @frame_list: Circular array to store the debug info
 * @frame_list_size: Size of circular array @frame_list
 * @next_index: The index at which information about next frame will be logged
 * @wrap_aroud: Flag to indicate whether wrap around occurred when logging
 * debug information to @frame_list
 * @stats: Stats related to scheduler
 */
struct reo_scheduler_debug_info {
	struct reo_scheduler_debug_frame_info *frame_list;
	uint16_t frame_list_size;
	int next_index;
	bool wrap_aroud;
	struct reo_scheduler_stats stats;
};
#endif /* WLAN_MGMT_RX_REO_DEBUG_SUPPORT */

/**
 * struct mgmt_rx_reo_context - This structure holds the info required for
 * management rx-reordering. Reordering is done across all the psocs.
 * So there should be only one instance of this structure defined.
 * @ingress_list: Ingress list object
 * @egress_list: Egress list object
 * @reo_algo_entry_lock: Spin lock to protect reo algorithm entry critical
 * section execution
 * @frame_release_lock: Spin lock to serialize the frame delivery to the
 * upper layers. This could prevent race conditions like the one given in
 * the following example.
 * Lets take an example of 2 links (Link A & B) and each has received
 * a management frame A1(deauth) and B1(auth) such that MLO global time
 * stamp of A1 < MLO global time stamp of B1. Host is concurrently
 * executing "mgmt_rx_reo_list_release_entries" for A1 and B1 in
 * 2 different CPUs. It is possible that frame B1 gets processed by
 * upper layers before frame A1 and this could result in unwanted
 * disconnection. Hence it is required to serialize the delivery
 * of management frames to upper layers in the strict order of MLO
 * global time stamp.
 * @sim_context: Management rx-reorder simulation context
 * @ingress_debug_info_init_count: Initialization count of
 * object @ingress_frame_debug_info
 * @ingress_frame_debug_info: Debug object to log incoming frames
 * @egress_frame_debug_info: Debug object to log outgoing frames
 * @egress_debug_info_init_count: Initialization count of
 * object @egress_frame_debug_info
 * @scheduler_debug_info_init_count: Initialization count of
 * object @scheduler_debug_info
 * @scheduler_debug_info: Debug object to log scheduler debug info
 * @simulation_in_progress: Flag to indicate whether simulation is
 * in progress
 * @mlo_grp_id: MLO Group ID which it belongs to
 * @context_id: Context identifier
 */
struct mgmt_rx_reo_context {
	struct mgmt_rx_reo_ingress_list ingress_list;
	struct mgmt_rx_reo_egress_list egress_list;
	qdf_spinlock_t reo_algo_entry_lock;
	qdf_spinlock_t frame_release_lock;
#ifdef WLAN_MGMT_RX_REO_SIM_SUPPORT
	struct mgmt_rx_reo_sim_context sim_context;
#endif /* WLAN_MGMT_RX_REO_SIM_SUPPORT */
#ifdef WLAN_MGMT_RX_REO_DEBUG_SUPPORT
	qdf_atomic_t ingress_debug_info_init_count;
	struct  reo_ingress_debug_info ingress_frame_debug_info;
	qdf_atomic_t egress_debug_info_init_count;
	struct  reo_egress_debug_info egress_frame_debug_info;
	qdf_atomic_t scheduler_debug_info_init_count;
	struct  reo_scheduler_debug_info scheduler_debug_info;
#endif /* WLAN_MGMT_RX_REO_DEBUG_SUPPORT */
	bool simulation_in_progress;
	uint8_t mlo_grp_id;
	qdf_atomic_t context_id;
};

/**
 * struct mgmt_rx_reo_frame_descriptor - Frame Descriptor used to describe
 * a management frame in mgmt rx reo module.
 * @type: Frame descriptor type
 * @frame_type: frame type
 * @frame_subtype: frame subtype
 * @nbuf: nbuf corresponding to this frame
 * @rx_params: Management rx event parameters
 * @wait_count: Wait counts for the frame
 * @ingress_timestamp: Host time stamp when the frames enters the reorder
 * algorithm
 * @is_stale: Indicates whether this frame is stale. Any frame older than the
 * last frame delivered to upper layer is a stale frame. Stale frames should not
 * be delivered to the upper layers. These frames can be discarded after
 * updating the host snapshot and wait counts of entries currently residing in
 * the reorder list.
 * @zero_wait_count_rx: Indicates whether this frame's wait count was
 * zero when received by host
 * @immediate_delivery: Indicates whether this frame can be delivered
 * immediately to the upper layers
 * @ingress_list_size_rx: Size of the ingress list when this frame is
 * received (before updating the ingress list based on this frame).
 * @ingress_list_insertion_pos: Position in the ingress list where this
 * frame is going to get inserted (Applicable for only host consumed frames)
 * @egress_list_size_rx: Size of the egress list when this frame is
 * added to the egress list
 * @egress_list_insertion_pos: Position in the egress list where this
 * frame is going to get inserted
 * @shared_snapshots: snapshots shared b/w host and target
 * @host_snapshot: host snapshot
 * @is_parallel_rx: Indicates that this frame is received in parallel to the
 * last frame which is delivered to the upper layer.
 * @queued_list: Type of list in which the current frame is queued
 * @pkt_ctr_delta: Packet counter delta of the current and last frame
 * @reo_required: Indicates whether reorder is required for the current frame.
 * If reorder is not required, current frame will just be used for updating the
 * wait count of frames already part of the reorder list.
 * @last_delivered_frame: Stores the information about the last frame delivered
 * to the upper layer
 * @reo_params_copy: Copy of @rx_params->reo_params structure
 * @drop_reason: Reason for dropping the frame
 * @drop: Indicates whether the frame has to be dropped
 */
struct mgmt_rx_reo_frame_descriptor {
	enum mgmt_rx_reo_frame_descriptor_type type;
	uint8_t frame_type;
	uint8_t frame_subtype;
	qdf_nbuf_t nbuf;
	struct mgmt_rx_event_params *rx_params;
	struct mgmt_rx_reo_wait_count wait_count;
	uint64_t ingress_timestamp;
	bool is_stale;
	bool zero_wait_count_rx;
	bool immediate_delivery;
	int16_t ingress_list_size_rx;
	int16_t ingress_list_insertion_pos;
	int16_t egress_list_size_rx;
	int16_t egress_list_insertion_pos;
	struct mgmt_rx_reo_snapshot_params shared_snapshots
			[MAX_MLO_LINKS][MGMT_RX_REO_SHARED_SNAPSHOT_MAX];
	struct mgmt_rx_reo_snapshot_params host_snapshot[MAX_MLO_LINKS];
	bool is_parallel_rx;
	enum mgmt_rx_reo_list_type queued_list;
	int pkt_ctr_delta;
	bool reo_required;
	struct mgmt_rx_reo_frame_info last_delivered_frame;
	struct mgmt_rx_reo_params reo_params_copy;
	enum mgmt_rx_reo_ingress_drop_reason drop_reason;
	bool drop;
};

/**
 * mgmt_rx_reo_list_overflowed() - Helper API to check whether mgmt rx reorder
 * list overflowed
 * @reo_list: Pointer to management rx reorder list
 *
 * Return: true or false
 */
static inline bool
mgmt_rx_reo_list_overflowed(struct mgmt_rx_reo_list *reo_list)
{
	if (!reo_list) {
		mgmt_rx_reo_err("reo list is null");
		return false;
	}

	return (qdf_list_size(&reo_list->list) > reo_list->max_list_size);
}

/**
 * mgmt_rx_reo_get_context_from_ingress_list() - Helper API to get pointer to
 * management rx reorder context from pointer to management rx reo ingress list
 * @ingress_list: Pointer to management rx reo ingress list
 *
 * Return: Pointer to management rx reorder context
 */
static inline struct mgmt_rx_reo_context *
mgmt_rx_reo_get_context_from_ingress_list
		(const struct mgmt_rx_reo_ingress_list *ingress_list) {
	if (!ingress_list) {
		mgmt_rx_reo_err("ingress list is null");
		return NULL;
	}

	return qdf_container_of(ingress_list, struct mgmt_rx_reo_context,
				ingress_list);
}

/**
 * mgmt_rx_reo_get_context_from_egress_list() - Helper API to get pointer to
 * management rx reorder context from pointer to management rx reo egress list
 * @egress_list: Pointer to management rx reo egress list
 *
 * Return: Pointer to management rx reorder context
 */
static inline struct mgmt_rx_reo_context *
mgmt_rx_reo_get_context_from_egress_list
			(const struct mgmt_rx_reo_egress_list *egress_list) {
	if (!egress_list) {
		mgmt_rx_reo_err("Egress list is null");
		return NULL;
	}

	return qdf_container_of(egress_list, struct mgmt_rx_reo_context,
				egress_list);
}

/**
 * mgmt_rx_reo_get_global_ts() - Helper API to get global time stamp
 * corresponding to the mgmt rx event
 * @rx_params: Management rx event params
 *
 * Return: global time stamp corresponding to the mgmt rx event
 */
static inline uint32_t
mgmt_rx_reo_get_global_ts(struct mgmt_rx_event_params *rx_params)
{
	if (!rx_params) {
		mgmt_rx_reo_err("rx params is null");
		return 0;
	}

	if (!rx_params->reo_params) {
		mgmt_rx_reo_err("reo params is null");
		return 0;
	}

	return rx_params->reo_params->global_timestamp;
}

/**
 * mgmt_rx_reo_get_start_ts() - Helper API to get start time stamp of the frame
 * @rx_params: Management rx event params
 *
 * Return: start time stamp of the frame
 */
static inline uint32_t
mgmt_rx_reo_get_start_ts(struct mgmt_rx_event_params *rx_params)
{
	if (!rx_params) {
		mgmt_rx_reo_err("rx params is null");
		return 0;
	}

	if (!rx_params->reo_params) {
		mgmt_rx_reo_err("reo params is null");
		return 0;
	}

	return rx_params->reo_params->start_timestamp;
}

/**
 * mgmt_rx_reo_get_end_ts() - Helper API to get end time stamp of the frame
 * @rx_params: Management rx event params
 *
 * Return: end time stamp of the frame
 */
static inline uint32_t
mgmt_rx_reo_get_end_ts(struct mgmt_rx_event_params *rx_params)
{
	if (!rx_params) {
		mgmt_rx_reo_err("rx params is null");
		return 0;
	}

	if (!rx_params->reo_params) {
		mgmt_rx_reo_err("reo params is null");
		return 0;
	}

	return rx_params->reo_params->end_timestamp;
}

/**
 * mgmt_rx_reo_get_duration_us() - Helper API to get the duration of the frame
 * in us
 * @rx_params: Management rx event params
 *
 * Return: Duration of the frame in us
 */
static inline uint32_t
mgmt_rx_reo_get_duration_us(struct mgmt_rx_event_params *rx_params)
{
	if (!rx_params) {
		mgmt_rx_reo_err("rx params is null");
		return 0;
	}

	if (!rx_params->reo_params) {
		mgmt_rx_reo_err("reo params is null");
		return 0;
	}

	return rx_params->reo_params->duration_us;
}

/**
 * mgmt_rx_reo_get_pkt_counter() - Helper API to get packet counter
 * corresponding to the mgmt rx event
 * @rx_params: Management rx event params
 *
 * Return: Management packet counter corresponding to the mgmt rx event
 */
static inline uint16_t
mgmt_rx_reo_get_pkt_counter(struct mgmt_rx_event_params *rx_params)
{
	if (!rx_params) {
		mgmt_rx_reo_err("rx params is null");
		return 0;
	}

	if (!rx_params->reo_params) {
		mgmt_rx_reo_err("reo params is null");
		return 0;
	}

	return rx_params->reo_params->mgmt_pkt_ctr;
}

/**
 * mgmt_rx_reo_get_link_id() - Helper API to get link id corresponding to the
 * mgmt rx event
 * @rx_params: Management rx event params
 *
 * Return: link id corresponding to the mgmt rx event
 */
static inline uint8_t
mgmt_rx_reo_get_link_id(struct mgmt_rx_event_params *rx_params)
{
	if (!rx_params) {
		mgmt_rx_reo_err("rx params is null");
		return 0;
	}

	if (!rx_params->reo_params) {
		mgmt_rx_reo_err("reo params is null");
		return 0;
	}

	return rx_params->reo_params->link_id;
}

/**
 * mgmt_rx_reo_get_mlo_grp_id() - Helper API to get MLO Group id
 * corresponding to the mgmt rx event
 * @rx_params: Management rx event params
 *
 * Return: MLO group id corresponding to the mgmt rx event
 */
static inline uint8_t
mgmt_rx_reo_get_mlo_grp_id(struct mgmt_rx_event_params *rx_params)
{
	if (!rx_params) {
		mgmt_rx_reo_err("rx params is null");
		return 0;
	}

	if (!rx_params->reo_params) {
		mgmt_rx_reo_err("reo params is null");
		return 0;
	}

	return rx_params->reo_params->mlo_grp_id;
}

/**
 * mgmt_rx_reo_get_pdev_id() - Helper API to get pdev id corresponding to the
 * mgmt rx event
 * @rx_params: Management rx event params
 *
 * Return: pdev id corresponding to the mgmt rx event
 */
static inline uint8_t
mgmt_rx_reo_get_pdev_id(struct mgmt_rx_event_params *rx_params)
{
	if (!rx_params) {
		mgmt_rx_reo_err("rx params is null");
		return 0;
	}

	if (!rx_params->reo_params) {
		mgmt_rx_reo_err("reo params is null");
		return 0;
	}

	return rx_params->reo_params->pdev_id;
}

/**
 * mgmt_rx_reo_init_context() - Initialize the management rx-reorder context
 * @ml_grp_id: MLO Group ID to be initialized
 *
 * API to initialize each global management rx-reorder context object per group
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_init_context(uint8_t ml_grp_id);

/**
 * mgmt_rx_reo_deinit_context() - De initialize the management rx-reorder
 * context
 * @ml_grp_id: MLO Group ID to be deinitialized
 *
 * API to de initialize each global management rx-reorder context object per
 * group
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_deinit_context(uint8_t ml_grp_id);

/**
 * mgmt_rx_reo_is_simulation_in_progress() - API to check whether
 * simulation is in progress
 * @ml_grp_id: MLO group id of mgmt rx reo
 *
 * Return: true if simulation is in progress, else false
 */
bool
mgmt_rx_reo_is_simulation_in_progress(uint8_t ml_grp_id);

/**
 * mgmt_rx_reo_print_ingress_frame_stats() - Helper API to print
 * stats related to incoming management frames
 * @ml_grp_id: MLO group id of mgmt rx reo
 *
 * This API prints stats related to management frames entering management
 * Rx reorder module.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_print_ingress_frame_stats(uint8_t ml_grp_id);

/**
 * mgmt_rx_reo_print_ingress_frame_info() - Print the debug information
 * about the latest frames entered the reorder module
 * @ml_grp_id: MLO group id of mgmt rx reo
 * @num_frames: Number of frames for which the debug information is to be
 * printed. If @num_frames is 0, then debug information about all the frames
 * in the ring buffer will be  printed.
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS
mgmt_rx_reo_print_ingress_frame_info(uint8_t ml_grp_id, uint16_t num_frames);

/**
 * mgmt_rx_reo_print_egress_frame_stats() - Helper API to print
 * stats related to outgoing management frames
 * @ml_grp_id: MLO group id of mgmt rx reo
 *
 * This API prints stats related to management frames exiting management
 * Rx reorder module.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_print_egress_frame_stats(uint8_t ml_grp_id);

/**
 * mgmt_rx_reo_print_egress_frame_info() - Print the debug information
 * about the latest frames leaving the reorder module
 * @ml_grp_id: MLO group id of mgmt rx reo
 * @num_frames: Number of frames for which the debug information is to be
 * printed. If @num_frames is 0, then debug information about all the frames
 * in the ring buffer will be  printed.
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS
mgmt_rx_reo_print_egress_frame_info(uint8_t ml_grp_id, uint16_t num_frames);

#ifdef WLAN_MGMT_RX_REO_SIM_SUPPORT
/**
 * mgmt_rx_reo_sim_start() - Helper API to start management Rx reorder
 * simulation
 * @ml_grp_id: MLO group id of mgmt rx reo
 *
 * This API starts the simulation framework which mimics the management frame
 * generation by target. MAC HW is modelled as a kthread. FW and host layers
 * are modelled as an ordered work queues.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_sim_start(uint8_t ml_grp_id);

/**
 * mgmt_rx_reo_sim_stop() - Helper API to stop management Rx reorder
 * simulation
 * @ml_grp_id: MLO group id of mgmt rx reo
 *
 * This API stops the simulation framework which mimics the management frame
 * generation by target. MAC HW is modelled as a kthread. FW and host layers
 * are modelled as an ordered work queues.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_sim_stop(uint8_t ml_grp_id);

/**
 * mgmt_rx_reo_sim_process_rx_frame() - API to process the management frame
 * in case of simulation
 * @pdev: pointer to pdev object
 * @buf: pointer to management frame buffer
 * @mgmt_rx_params: pointer to management frame parameters
 *
 * This API validates whether the reo algorithm has reordered frames correctly.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_sim_process_rx_frame(struct wlan_objmgr_pdev *pdev,
				 qdf_nbuf_t buf,
				 struct mgmt_rx_event_params *mgmt_rx_params);

/**
 * mgmt_rx_reo_sim_get_snapshot_address() - Get snapshot address
 * @pdev: pointer to pdev
 * @id: snapshot identifier
 * @address: pointer to snapshot address
 *
 * Helper API to get address of snapshot @id for pdev @pdev. For simulation
 * purpose snapshots are allocated in the simulation context object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_sim_get_snapshot_address(
			struct wlan_objmgr_pdev *pdev,
			enum mgmt_rx_reo_shared_snapshot_id id,
			struct mgmt_rx_reo_shared_snapshot **address);

/**
 * mgmt_rx_reo_sim_pdev_object_create_notification() - pdev create handler for
 * management rx-reorder simulation framework
 * @pdev: pointer to pdev object
 *
 * This function gets called from object manager when pdev is being created and
 * builds the link id to pdev map in simulation context object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_sim_pdev_object_create_notification(struct wlan_objmgr_pdev *pdev);

/**
 * mgmt_rx_reo_sim_pdev_object_destroy_notification() - pdev destroy handler for
 * management rx-reorder simulation framework
 * @pdev: pointer to pdev object
 *
 * This function gets called from object manager when pdev is being destroyed
 * and destroys the link id to pdev map in simulation context.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_sim_pdev_object_destroy_notification(struct wlan_objmgr_pdev *pdev);

/**
 * mgmt_rx_reo_sim_get_mlo_link_id_from_pdev() - Helper API to get the MLO HW
 * link id from the pdev object.
 * @pdev: Pointer to pdev object
 *
 * This API is applicable for simulation only. A static map from MLO HW link id
 * to the pdev object is created at the init time. This API uses the map to
 * find the MLO HW link id for a given pdev.
 *
 * Return: On success returns the MLO HW link id corresponding to the pdev
 * object. On failure returns -1.
 */
int8_t
mgmt_rx_reo_sim_get_mlo_link_id_from_pdev(struct wlan_objmgr_pdev *pdev);

/**
 * mgmt_rx_reo_sim_get_pdev_from_mlo_link_id() - Helper API to get the pdev
 * object from the MLO HW link id.
 * @mlo_link_id: MLO HW link id
 * @refdbgid: Reference debug id
 *
 * This API is applicable for simulation only. A static map from MLO HW link id
 * to the pdev object is created at the init time. This API uses the map to
 * find the pdev object from the MLO HW link id.
 *
 * Return: On success returns the pdev object corresponding to the MLO HW
 * link id. On failure returns NULL.
 */
struct wlan_objmgr_pdev *
mgmt_rx_reo_sim_get_pdev_from_mlo_link_id(uint8_t mlo_link_id,
					  wlan_objmgr_ref_dbgid refdbgid);
#endif /* WLAN_MGMT_RX_REO_SIM_SUPPORT */

/**
 * is_mgmt_rx_reo_required() - Whether MGMT REO required for this frame/event
 * @pdev: pdev for which this frame/event is intended
 * @desc: Descriptor corresponding to this frame/event
 *
 * Return: true if REO is required; else false
 */
static inline bool is_mgmt_rx_reo_required(
			struct wlan_objmgr_pdev *pdev,
			struct mgmt_rx_reo_frame_descriptor *desc)
{
	/**
	 * NOTE: Implementing a simple policy based on the number of MLO vdevs
	 * in the given pdev.
	 */
	return  wlan_pdev_get_mlo_vdev_count(pdev);
}

/**
 * wlan_mgmt_rx_reo_algo_entry() - Entry point to the MGMT Rx REO algorithm for
 * a given MGMT frame/event.
 * @pdev: pdev for which this frame/event is intended
 * @desc: Descriptor corresponding to this frame/event
 * @is_queued: Whether this frame/event is queued in the REO list
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS
wlan_mgmt_rx_reo_algo_entry(struct wlan_objmgr_pdev *pdev,
			    struct mgmt_rx_reo_frame_descriptor *desc,
			    bool *is_queued);

/**
 * mgmt_rx_reo_validate_mlo_link_info() - Validate the MLO HW link info
 * obtained from the global shared memory arena
 * @psoc: Pointer to psoc object
 *
 * Validate the following MLO HW link related information extracted from
 * management Rx reorder related TLVs in global shared memory arena.
 *         1. Number of active MLO HW links
 *         2. Valid MLO HW link bitmap
 *
 * Return: QDF_STATUS of operation
 */
QDF_STATUS
mgmt_rx_reo_validate_mlo_link_info(struct wlan_objmgr_psoc *psoc);

/**
 * mgmt_rx_reo_release_frames() - Release management frames which are ready
 * for delivery
 * @mlo_grp_id: MLO group ID
 * @link_bitmap: Link bitmap
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mgmt_rx_reo_release_frames(uint8_t mlo_grp_id, uint32_t link_bitmap);
#endif /* WLAN_MGMT_RX_REO_SUPPORT */
#endif /* _WLAN_MGMT_TXRX_RX_REO_I_H */

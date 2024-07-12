/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains T2LM APIs
 */

#ifndef _WLAN_MLO_T2LM_H_
#define _WLAN_MLO_T2LM_H_

#include <wlan_cmn_ieee80211.h>
#include <wlan_mlo_mgr_public_structs.h>

struct mlo_vdev_host_tid_to_link_map_resp;
struct wlan_mlo_dev_context;

/* Max T2LM TIDS count */
#define T2LM_MAX_NUM_TIDS 8

/* Max T2LM callback handlers */
#define MAX_T2LM_HANDLERS 50

#define T2LM_EXPECTED_DURATION_MAX_VALUE 0xFFFFFF

/* Mapping switch time represented as bits 10 to 25 of the TSF value */
#define WLAN_T2LM_MAPPING_SWITCH_TSF_BITS 0x3FFFC00

/* There is a delay involved to receive and process the beacon/probe response
 * T2LM IE from AP. To match mapping switch timer expiry in both AP and STA,
 * advance timer expiry in STA by 100ms (= 98 * 1024 / 1000 = 100).
 */
#define WLAN_T2LM_MAPPING_SWITCH_TIME_DELAY 98

/**
 * enum wlan_t2lm_direction - Indicates the direction for which TID-to-link
 * mapping is available.
 *
 * @WLAN_T2LM_DL_DIRECTION: Downlink
 * @WLAN_T2LM_UL_DIRECTION: Uplink
 * @WLAN_T2LM_BIDI_DIRECTION: Both downlink and uplink
 * @WLAN_T2LM_MAX_DIRECTION: Max direction, this is used only internally
 * @WLAN_T2LM_INVALID_DIRECTION: Invalid, this is used only internally to check
 *                               if the mapping present in wlan_t2lm_info
 *                               structure is valid or not.
 */
enum wlan_t2lm_direction {
	WLAN_T2LM_DL_DIRECTION,
	WLAN_T2LM_UL_DIRECTION,
	WLAN_T2LM_BIDI_DIRECTION,
	WLAN_T2LM_MAX_DIRECTION,
	WLAN_T2LM_INVALID_DIRECTION,
};

/**
 * struct wlan_t2lm_info - TID-to-Link mapping information for the frames
 * transmitted on the uplink, downlink and bidirectional.
 *
 * @direction:  0 - Downlink, 1 - uplink 2 - Both uplink and downlink
 * @default_link_mapping: value 1 indicates the default T2LM, where all the TIDs
 *                        are mapped to all the links.
 *                        value 0 indicates the preferred T2LM mapping
 * @mapping_switch_time_present: Indicates if mapping switch time field present
 *                               in the T2LM IE
 * @expected_duration_present: Indicates if expected duration present in the
 *                             T2LM IE
 * @mapping_switch_time: Mapping switch time of this T2LM IE
 * @expected_duration: Expected duration of this T2LM IE
 * @ieee_link_map_tid: Indicates ieee link id mapping of all the TIDS
 * @hw_link_map_tid: Indicates hw link id mapping of all the TIDS
 * @timer_started: flag to check if T2LM timer is started for this T2LM IE
 * @link_mapping_size: value 1 indicates the length of Link Mapping Of TIDn
 *                     field is 1 octet, value 0 indicates the length of the
 *                     Link Mapping of TIDn field is 2 octets
 */
struct wlan_t2lm_info {
	enum wlan_t2lm_direction direction;
	bool default_link_mapping;
	bool mapping_switch_time_present;
	bool expected_duration_present;
	uint16_t mapping_switch_time;
	uint32_t expected_duration;
	uint16_t ieee_link_map_tid[T2LM_MAX_NUM_TIDS];
	uint16_t hw_link_map_tid[T2LM_MAX_NUM_TIDS];
	bool timer_started;
	bool link_mapping_size;
};

/**
 * enum wlan_t2lm_category - T2LM category
 *
 * @WLAN_T2LM_CATEGORY_NONE: none
 * @WLAN_T2LM_CATEGORY_REQUEST: T2LM request
 * @WLAN_T2LM_CATEGORY_RESPONSE: T2LM response
 * @WLAN_T2LM_CATEGORY_TEARDOWN: T2LM teardown
 * @WLAN_T2LM_CATEGORY_INVALID: Invalid
 */
enum wlan_t2lm_category {
	WLAN_T2LM_CATEGORY_NONE = 0,
	WLAN_T2LM_CATEGORY_REQUEST = 1,
	WLAN_T2LM_CATEGORY_RESPONSE = 2,
	WLAN_T2LM_CATEGORY_TEARDOWN = 3,
	WLAN_T2LM_CATEGORY_INVALID,
};

/**
 * enum wlan_t2lm_tx_status - Status code applicable for the T2LM frames
 * transmitted by the current peer.
 *
 * @WLAN_T2LM_TX_STATUS_NONE: Status code is not applicable
 * @WLAN_T2LM_TX_STATUS_SUCCESS: AP/STA successfully transmitted the T2LM frame
 * @WLAN_T2LM_TX_STATUS_FAILURE: Tx failure received from the FW.
 * @WLAN_T2LM_TX_STATUS_RX_TIMEOUT: T2LM response frame not received from the
 *                              peer for the transmitted T2LM request frame.
 * @WLAN_T2LM_TX_STATUS_INVALID: Invalid status code
 */
enum wlan_t2lm_tx_status {
	WLAN_T2LM_TX_STATUS_NONE = 0,
	WLAN_T2LM_TX_STATUS_SUCCESS = 1,
	WLAN_T2LM_TX_STATUS_FAILURE = 2,
	WLAN_T2LM_TX_STATUS_RX_TIMEOUT = 3,
	WLAN_T2LM_TX_STATUS_INVALID,
};

/**
 * enum wlan_t2lm_resp_frm_type - T2LM status corresponds to T2LM response frame
 *
 * @WLAN_T2LM_RESP_TYPE_SUCCESS: T2LM mapping provided in the T2LM request is
 *                       accepted either by the AP or STA
 * @WLAN_T2LM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING: T2LM Request denied because
 *                       the requested TID-to-link mapping is unacceptable.
 * @WLAN_T2LM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING: T2LM Request rejected and
 *                       preferred TID-to-link mapping is suggested.
 * @WLAN_T2LM_RESP_TYPE_INVALID: Status code is not applicable.
 */
enum wlan_t2lm_resp_frm_type {
	WLAN_T2LM_RESP_TYPE_SUCCESS = 0,
	WLAN_T2LM_RESP_TYPE_DENIED_TID_TO_LINK_MAPPING = 133,
	WLAN_T2LM_RESP_TYPE_PREFERRED_TID_TO_LINK_MAPPING = 134,
	WLAN_T2LM_RESP_TYPE_INVALID,
};

/**
 * enum wlan_t2lm_enable - TID-to-link negotiation supported by the mlo peer
 *
 * @WLAN_T2LM_NOT_SUPPORTED: T2LM is not supported by the MLD
 * @WLAN_T2LM_MAP_ALL_TIDS_TO_SAME_LINK_SET: MLD only supports the mapping of
 *    all TIDs to the same link set.
 * @WLAN_T2LM_MAP_RESERVED: reserved value
 * @WLAN_T2LM_MAP_EACH_TID_TO_SAME_OR_DIFFERENET_LINK_SET: MLD supports the
 *    mapping of each TID to the same or different link set (Disjoint mapping).
 * @WLAN_T2LM_ENABLE_INVALID: invalid
 */
enum wlan_t2lm_enable {
	WLAN_T2LM_NOT_SUPPORTED = 0,
	WLAN_T2LM_MAP_ALL_TIDS_TO_SAME_LINK_SET = 1,
	WLAN_T2LM_MAP_RESERVED = 2,
	WLAN_T2LM_MAP_EACH_TID_TO_SAME_OR_DIFFERENET_LINK_SET = 3,
	WLAN_T2LM_ENABLE_INVALID,
};

/**
 * struct wlan_prev_t2lm_negotiated_info - Previous successful T2LM negotiation
 * is saved here.
 *
 * @dialog_token: Save the dialog token used in T2LM request and response frame.
 * @t2lm_info: Provides the TID to LINK mapping information
 */
struct wlan_prev_t2lm_negotiated_info {
	uint16_t dialog_token;
	struct wlan_t2lm_info t2lm_info[WLAN_T2LM_MAX_DIRECTION];
};

/**
 * struct wlan_t2lm_onging_negotiation_info - Current ongoing T2LM negotiation
 * (information about transmitted T2LM request/response frame)
 *
 * @category: T2LM category as T2LM request frame
 * @dialog_token: Save the dialog token used in T2LM request and response frame.
 * @t2lm_info: Provides the TID-to-link mapping info for UL/DL/BiDi
 * @t2lm_tx_status: Status code corresponds to the transmitted T2LM frames
 * @t2lm_resp_type: T2LM status corresponds to T2LM response frame.
 */
struct wlan_t2lm_onging_negotiation_info {
	enum wlan_t2lm_category category;
	uint8_t dialog_token;
	struct wlan_t2lm_info t2lm_info[WLAN_T2LM_MAX_DIRECTION];
	enum wlan_t2lm_tx_status t2lm_tx_status;
	enum wlan_t2lm_resp_frm_type t2lm_resp_type;
};

/**
 * struct wlan_mlo_peer_t2lm_policy - TID-to-link mapping information
 *
 * @self_gen_dialog_token: self generated dialog token used to send T2LM request
 *                         frame;
 * @t2lm_enable_val: TID-to-link enable value supported by this peer.
 * @t2lm_negotiated_info: Previous successful T2LM negotiation is saved here.
 * @ongoing_tid_to_link_mapping: This has the ongoing TID-to-link mapping info
 *                               transmitted by this peer to the connected peer.
 */
struct wlan_mlo_peer_t2lm_policy {
	uint8_t self_gen_dialog_token;
	enum wlan_t2lm_enable t2lm_enable_val;
	struct wlan_prev_t2lm_negotiated_info t2lm_negotiated_info;
	struct wlan_t2lm_onging_negotiation_info ongoing_tid_to_link_mapping;
};

/**
 * struct wlan_mlo_t2lm_ie - T2LM information
 *
 * @disabled_link_bitmap: Bitmap of disabled links. This is used to update the
 *                        disabled link field of RNR IE
 * @t2lm: T2LM info structure
 */
struct wlan_mlo_t2lm_ie {
	uint16_t disabled_link_bitmap;
	struct wlan_t2lm_info t2lm;
};

/*
 * In a beacon or probe response frame, at max two T2LM IEs can be present
 * first one to represent the already existing mapping and the other one
 * represents the new T2LM mapping that is yet to establish.
 */
#define WLAN_MAX_T2LM_IE 2
/**
 * struct wlan_t2lm_timer - T2LM timer information
 *
 * @t2lm_timer: T2LM timer
 * @timer_interval: T2LM Timer value
 * @timer_started: T2LM timer started or not
 * @timer_out_time: T2LM timer target out time
 * @t2lm_dev_lock: lock to access struct
 */
struct wlan_t2lm_timer {
	qdf_timer_t t2lm_timer;
	uint32_t timer_interval;
	uint32_t timer_out_time;
	bool timer_started;
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t t2lm_dev_lock;
#else
	qdf_mutex_t t2lm_dev_lock;
#endif
};

struct wlan_mlo_dev_context;

/**
 * typedef wlan_mlo_t2lm_link_update_handler - T2LM handler API to notify the
 * link update.
 * @vdev: Pointer to vdev context
 * @t2lm: Pointer to wlan_t2lm_info
 *
 * Return: QDF_STATUS
 */
typedef QDF_STATUS (*wlan_mlo_t2lm_link_update_handler)(
					struct wlan_objmgr_vdev *vdev,
					struct wlan_t2lm_info *t2lm);

/**
 * struct wlan_t2lm_context - T2LM IE information
 *
 * @established_t2lm: Indicates the already established broadcast T2LM IE
 *                    advertised by the AP in beacon/probe response frames.
 *                    In this T2LM IE, expected duration flag is set to 1 and
 *                    mapping switch time present flag is set to 0 when the
 *                    mapping is non-default.
 * @upcoming_t2lm: Indicates the new broadcast T2LM IE advertised by the AP in
 *                 beacon/probe response frames. STA needs to use this mapping
 *                 when expected duration in the established T2LM is expires.
 * @t2lm_timer: T2LM timer information
 * @t2lm_dev_lock: t2lm dev context lock
 * @tsf: time sync func value received via beacon
 * @link_update_handler: handler to update T2LM link
 * @is_valid_handler: T2LM handler is valid or not
 */
struct wlan_t2lm_context {
	struct wlan_mlo_t2lm_ie established_t2lm;
	struct wlan_mlo_t2lm_ie upcoming_t2lm;
	struct wlan_t2lm_timer t2lm_timer;
#ifdef WLAN_MLO_USE_SPINLOCK
	qdf_spinlock_t t2lm_dev_lock;
#else
	qdf_mutex_t t2lm_dev_lock;
#endif
	uint64_t tsf;
	wlan_mlo_t2lm_link_update_handler
		link_update_handler[MAX_T2LM_HANDLERS];
	bool is_valid_handler[MAX_T2LM_HANDLERS];
};

#ifdef WLAN_FEATURE_11BE

#define t2lm_alert(format, args...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_T2LM, format, ## args)

#define t2lm_err(format, args...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_T2LM, format, ## args)

#define t2lm_warn(format, args...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_T2LM, format, ## args)

#define t2lm_info(format, args...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_T2LM, format, ## args)

#define t2lm_debug(format, args...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_T2LM, format, ## args)

#define t2lm_rl_debug(format, args...) \
	QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_T2LM, format, ## args)

#define WLAN_T2LM_MAX_NUM_LINKS 16

#ifdef WLAN_MLO_USE_SPINLOCK
/**
 * t2lm_dev_lock_create - Create T2LM device mutex/spinlock
 * @t2lm_ctx: T2LM context
 *
 * Creates mutex/spinlock
 *
 * Return: void
 */
static inline void
t2lm_dev_lock_create(struct wlan_t2lm_context *t2lm_ctx)
{
	qdf_spinlock_create(&t2lm_ctx->t2lm_dev_lock);
}

/**
 * t2lm_dev_lock_destroy - Destroy T2LM mutex/spinlock
 * @t2lm_ctx: T2LM context
 *
 * Destroy mutex/spinlock
 *
 * Return: void
 */
static inline void
t2lm_dev_lock_destroy(struct wlan_t2lm_context *t2lm_ctx)
{
	qdf_spinlock_destroy(&t2lm_ctx->t2lm_dev_lock);
}

/**
 * t2lm_dev_lock_acquire - acquire T2LM mutex/spinlock
 * @t2lm_ctx: T2LM context
 *
 * acquire mutex/spinlock
 *
 * return: void
 */
static inline
void t2lm_dev_lock_acquire(struct wlan_t2lm_context *t2lm_ctx)
{
	qdf_spin_lock_bh(&t2lm_ctx->t2lm_dev_lock);
}

/**
 * t2lm_dev_lock_release - release T2LM dev mutex/spinlock
 * @t2lm_ctx: T2LM context
 *
 * release mutex/spinlock
 *
 * return: void
 */
static inline
void t2lm_dev_lock_release(struct wlan_t2lm_context *t2lm_ctx)
{
	qdf_spin_unlock_bh(&t2lm_ctx->t2lm_dev_lock);
}
#else /* WLAN_MLO_USE_SPINLOCK */
static inline
void t2lm_dev_lock_create(struct wlan_t2lm_context *t2lm_ctx)
{
	qdf_mutex_create(&t2lm_ctx->t2lm_dev_lock);
}

static inline
void t2lm_dev_lock_destroy(struct wlan_t2lm_context *t2lm_ctx)
{
	qdf_mutex_destroy(&t2lm_ctx->t2lm_dev_lock);
}

static inline void t2lm_dev_lock_acquire(struct wlan_t2lm_context *t2lm_ctx)
{
	qdf_mutex_acquire(&t2lm_ctx->t2lm_dev_lock);
}

static inline void t2lm_dev_lock_release(struct wlan_t2lm_context *t2lm_ctx)
{
	qdf_mutex_release(&t2lm_ctx->t2lm_dev_lock);
}
#endif

/**
 * wlan_register_t2lm_link_update_notify_handler() - API to register the
 * handlers to receive link update notification
 * @handler: handler for T2LM link update
 * @mldev: Pointer to mlo context
 *
 * Return: Index on which handler is registered
 */
int wlan_register_t2lm_link_update_notify_handler(
		wlan_mlo_t2lm_link_update_handler handler,
		struct wlan_mlo_dev_context *mldev);

/**
 * wlan_unregister_t2lm_link_update_notify_handler() - API to unregister the
 * T2LM related handlers
 * @mldev: Pointer to mlo context
 * @index: Index on which the handler was registered
 *
 * Return: None
 */
void wlan_unregister_t2lm_link_update_notify_handler(
		struct wlan_mlo_dev_context *mldev, uint8_t index);

/**
 * wlan_mlo_dev_t2lm_notify_link_update() - API to call the registered handlers
 * when there is a link update happens using T2LM
 * @vdev: Pointer to vdev
 * @t2lm: Pointer to T2LM info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_dev_t2lm_notify_link_update(
		struct wlan_objmgr_vdev *vdev,
		struct wlan_t2lm_info *t2lm);

/**
 * wlan_mlo_parse_t2lm_ie() - API to parse the T2LM IE
 * @t2lm: Pointer to T2LM structure
 * @ie: Pointer to T2LM IE
 * @frame_len: Action Frame length
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_parse_t2lm_ie(
	struct wlan_t2lm_onging_negotiation_info *t2lm, uint8_t *ie,
	uint32_t frame_len);

/**
 * wlan_mlo_add_t2lm_ie() - API to add TID-to-link mapping IE
 * @frm: Pointer to buffer
 * @t2lm: Pointer to t2lm mapping structure
 * @vdev: Pointer to vdev structure
 *
 * Return: Updated frame pointer
 */
uint8_t *wlan_mlo_add_t2lm_ie(uint8_t *frm,
			      struct wlan_t2lm_onging_negotiation_info *t2lm,
			      struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_vdev_tid_to_link_map_event() - API to process the revceived T2LM
 * event.
 * @psoc: psoc object
 * @event: Pointer to received T2LM info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_vdev_tid_to_link_map_event(
			struct wlan_objmgr_psoc *psoc,
			struct mlo_vdev_host_tid_to_link_map_resp *event);

/**
 * wlan_mlo_parse_t2lm_action_frame() - API to parse T2LM action frame
 * @t2lm: Pointer to T2LM structure
 * @action_frm: Pointer to action frame
 * @frame_len: Action frame length
 * @category: T2LM action frame category
 *
 * Return: 0 - success, else failure
 */
int wlan_mlo_parse_t2lm_action_frame(
		struct wlan_t2lm_onging_negotiation_info *t2lm,
		struct wlan_action_frame *action_frm,
		uint32_t frame_len,
		enum wlan_t2lm_category category);

/**
 * wlan_mlo_add_t2lm_action_frame() - API to add T2LM action frame
 * @frm: Pointer to a frame to add T2LM IE
 * @args: T2LM action frame related info
 * @buf: Pointer to T2LM IE values
 * @category: T2LM action frame category
 *
 * Return: Pointer to the updated frame buffer
 */
uint8_t *wlan_mlo_add_t2lm_action_frame(
		uint8_t *frm, struct wlan_action_frame_args *args,
		uint8_t *buf, enum wlan_t2lm_category category);

/**
 * wlan_mlo_parse_bcn_prbresp_t2lm_ie() - API to parse the T2LM IE from beacon/
 * probe response frame
 * @t2lm_ctx: T2LM context
 * @ie: Pointer to T2LM IE
 * @frame_len: Frame length
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_parse_bcn_prbresp_t2lm_ie(
		struct wlan_t2lm_context *t2lm_ctx, uint8_t *ie,
		uint32_t frame_len);

/**
 * wlan_mlo_parse_t2lm_info() - Parse T2LM IE fields
 * @ie: Pointer to T2LM IE
 * @t2lm: Pointer to T2LM structure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlo_parse_t2lm_info(uint8_t *ie,
				    struct wlan_t2lm_info *t2lm);

/**
 * wlan_mlo_add_t2lm_info_ie() - Add T2LM IE for UL/DL/Bidirection
 * @frm: Pointer to buffer
 * @t2lm: Pointer to t2lm mapping structure
 * @vdev: Pointer to vdev structure
 *
 * Return: Updated frame pointer
 */
uint8_t *wlan_mlo_add_t2lm_info_ie(uint8_t *frm, struct wlan_t2lm_info *t2lm,
				   struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_t2lm_timer_init() - API to initialize t2lm timer
 * @vdev: Pointer to vdev
 *
 * Return: qdf status
 */
QDF_STATUS
wlan_mlo_t2lm_timer_init(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_t2lm_timer_deinit() - API to deinit t2lm timer
 * @vdev: Pointer to vdev
 *
 * Return: qdf status
 */
QDF_STATUS
wlan_mlo_t2lm_timer_deinit(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_t2lm_timer_start() - API to start T2LM timer
 * @vdev: Pointer to vdev
 * @interval: T2LM timer interval
 *
 * Return: qdf status
 */
QDF_STATUS
wlan_mlo_t2lm_timer_start(struct wlan_objmgr_vdev *vdev,
			  uint32_t interval);

/**
 * wlan_mlo_t2lm_timer_stop() - API to stop TID-to-link mapping timer
 * @vdev: Pointer to vdev
 *
 * Return: qdf status
 */
QDF_STATUS
wlan_mlo_t2lm_timer_stop(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_t2lm_timer_expiry_handler() - API to handle t2lm timer expiry
 * @vdev: Pointer to vdev structure
 *
 * Return: none
 */
void
wlan_mlo_t2lm_timer_expiry_handler(void *vdev);

/**
 * wlan_handle_t2lm_timer() - API to handle TID-to-link mapping timer
 * @vdev: Pointer to vdev
 *
 * Return: qdf status
 */
QDF_STATUS
wlan_handle_t2lm_timer(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_process_bcn_prbrsp_t2lm_ie() - API to process the received T2LM IE from
 * beacon/probe response.
 * @vdev: Pointer to vdev
 * @rx_t2lm_ie: Received T2LM IE
 * @tsf: Local TSF value
 *
 * Return QDF_STATUS
 */
QDF_STATUS wlan_process_bcn_prbrsp_t2lm_ie(struct wlan_objmgr_vdev *vdev,
					   struct wlan_t2lm_context *rx_t2lm_ie,
					   uint64_t tsf);

/**
 * wlan_send_tid_to_link_mapping() - API to send T2LM info received from beacon,
 * probe response or action frame to FW.
 *
 * @vdev: Pointer to vdev
 * @t2lm: T2LM info
 *
 * Return QDF_STATUS
 */
QDF_STATUS wlan_send_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev,
					 struct wlan_t2lm_info *t2lm);

/**
 * wlan_get_t2lm_mapping_status() - API to get T2LM info
 * @vdev: Pointer to vdev
 * @t2lm: T2LM info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_get_t2lm_mapping_status(struct wlan_objmgr_vdev *vdev,
					struct wlan_t2lm_info *t2lm);

/**
 * wlan_send_peer_level_tid_to_link_mapping() - API to send peer level T2LM info
 * negotiated using action frames to FW.
 *
 * @vdev: Pointer to vdev
 * @peer: pointer to peer
 *
 * Return QDF_STATUS
 */
QDF_STATUS
wlan_send_peer_level_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev,
					 struct wlan_objmgr_peer *peer);
#else
static inline QDF_STATUS wlan_mlo_parse_t2lm_ie(
	struct wlan_t2lm_onging_negotiation_info *t2lm, uint8_t *ie,
	uint32_t frame_len)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
int8_t *wlan_mlo_add_t2lm_ie(uint8_t *frm,
			     struct wlan_t2lm_onging_negotiation_info *t2lm,
			     struct wlan_objmgr_vdev *vdev)
{
	return frm;
}

static inline
int wlan_mlo_parse_t2lm_action_frame(
		struct wlan_t2lm_onging_negotiation_info *t2lm,
		struct wlan_action_frame *action_frm,
		uint32_t frame_len,
		enum wlan_t2lm_category category)
{
	return 0;
}

static inline
uint8_t *wlan_mlo_add_t2lm_action_frame(
		uint8_t *frm, struct wlan_action_frame_args *args,
		uint8_t *buf, enum wlan_t2lm_category category)
{
	return frm;
}

static inline
QDF_STATUS wlan_mlo_parse_bcn_prbresp_t2lm_ie(
		struct wlan_t2lm_context *t2lm_ctx, uint8_t *ie,
		uint32_t frame_len)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
QDF_STATUS wlan_mlo_parse_t2lm_info(uint8_t *ie,
				    struct wlan_t2lm_info *t2lm)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
uint8_t *wlan_mlo_add_t2lm_info_ie(uint8_t *frm, struct wlan_t2lm_info *t2lm,
				   struct wlan_objmgr_vdev *vdev)
{
	return frm;
}

static inline QDF_STATUS
wlan_mlo_t2lm_timer_init(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_mlo_t2lm_timer_deinit(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_mlo_t2lm_timer_start(struct wlan_objmgr_vdev *vdev,
			  uint32_t interval)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_mlo_t2lm_timer_stop(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline void
wlan_mlo_t2lm_timer_expiry_handler(void *vdev)
{}

static inline QDF_STATUS
wlan_handle_t2lm_timer(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
wlan_process_bcn_prbrsp_t2lm_ie(struct wlan_objmgr_vdev *vdev,
				struct wlan_t2lm_context *rx_t2lm_ie,
				uint64_t tsf)
{
	return QDF_STATUS_SUCCESS;
}

static inline
int wlan_register_t2lm_link_update_notify_handler(
		wlan_mlo_t2lm_link_update_handler handler,
		struct wlan_mlo_dev_context *mldev)
{
	return 0;
}

static inline
void wlan_unregister_t2lm_link_update_notify_handler(
		struct wlan_mlo_dev_context *mldev, uint8_t index)
{
}

static inline QDF_STATUS wlan_mlo_dev_t2lm_notify_link_update(
		struct wlan_objmgr_vdev *vdev,
		struct wlan_t2lm_info *t2lm)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS wlan_send_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev,
					 struct wlan_t2lm_info *t2lm)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_send_peer_level_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev,
					 struct wlan_objmgr_peer *peer)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_11BE */

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_FEATURE_11BE_MLO_ADV_FEATURE)
/**
 * wlan_clear_peer_level_tid_to_link_mapping() - API to clear peer level T2LM
 * info negotiated using action frames to FW.
 *
 * @vdev: Pointer to vdev
 *
 * Return: none
 */
void
wlan_clear_peer_level_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlo_link_disable_request_handler() - API to handle mlo link disable
 * request handler.
 *
 * @psoc: Pointer to psoc
 * @evt_params: MLO Link disable request params
 *
 * Return QDF_STATUS
 */
QDF_STATUS
wlan_mlo_link_disable_request_handler(struct wlan_objmgr_psoc *psoc,
				      void *evt_params);
#else
static inline void
wlan_clear_peer_level_tid_to_link_mapping(struct wlan_objmgr_vdev *vdev)
{
}

static inline QDF_STATUS
wlan_mlo_link_disable_request_handler(struct wlan_objmgr_psoc *psoc,
				      void *evt_params)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
#endif /* _WLAN_MLO_T2LM_H_ */

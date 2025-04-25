/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * DOC: contains ML STA link force active/inactive public API
 */
#ifndef _WLAN_MLO_LINK_FORCE_H_
#define _WLAN_MLO_LINK_FORCE_H_

#include <wlan_mlo_mgr_cmn.h>
#include <wlan_mlo_mgr_public_structs.h>
#include <wlan_mlo_mgr_link_switch.h>

/**
 * enum ml_nlink_change_event_type - Ml link state change trigger event
 * @ml_nlink_link_switch_start_evt: link switch start
 * @ml_nlink_link_switch_pre_completion_evt: link switch pre-completion
 * @ml_nlink_roam_sync_start_evt: roam sync start
 * @ml_nlink_roam_sync_completion_evt: roam sync completion
 * @ml_nlink_connect_start_evt: STA/CLI connect start
 * @ml_nlink_connect_completion_evt: STA/CLI connect completion
 * @ml_nlink_disconnect_start_evt: STA/CLI disconnect start
 * @ml_nlink_disconnect_completion_evt: STA/CLI disconnect completion
 * @ml_nlink_ap_started_evt: SAP/GO bss started
 * @ml_nlink_ap_stopped_evt: SAP/GO bss stopped
 * @ml_nlink_connection_updated_evt: connection home channel changed
 * @ml_nlink_tdls_request_evt: tdls request link enable/disable
 * @ml_nlink_vendor_cmd_request_evt: vendor command request
 */
enum ml_nlink_change_event_type {
	ml_nlink_link_switch_start_evt,
	ml_nlink_link_switch_pre_completion_evt,
	ml_nlink_roam_sync_start_evt,
	ml_nlink_roam_sync_completion_evt,
	ml_nlink_connect_start_evt,
	ml_nlink_connect_completion_evt,
	ml_nlink_disconnect_start_evt,
	ml_nlink_disconnect_completion_evt,
	ml_nlink_ap_started_evt,
	ml_nlink_ap_stopped_evt,
	ml_nlink_connection_updated_evt,
	ml_nlink_tdls_request_evt,
	ml_nlink_vendor_cmd_request_evt,
};

/**
 * enum link_control_modes - the types of MLO links state
 * control modes. This enum is internal mapping of
 * qca_wlan_vendor_link_state_control_modes.
 * @LINK_CONTROL_MODE_DEFAULT: MLO links state controlled
 * by the driver.
 * @LINK_CONTROL_MODE_USER: MLO links state controlled by
 * user space.
 * @LINK_CONTROL_MODE_MIXED: User space provides the
 * desired number of MLO links to operate in active state at any given time.
 * The driver will choose which MLO links should operate in the active state.
 * See enum qca_wlan_vendor_link_state for active state definition.
 */
enum link_control_modes {
	LINK_CONTROL_MODE_DEFAULT = 0,
	LINK_CONTROL_MODE_USER = 1,
	LINK_CONTROL_MODE_MIXED = 2,
};

/**
 * struct ml_nlink_change_event - connection change event data struct
 * @evt: event parameters
 * @link_switch: link switch start parameters
 * @tdls: tdls parameters
 * @vendor: vendor command set link parameters
 */
struct ml_nlink_change_event {
	union {
		struct {
			uint8_t curr_ieee_link_id;
			uint8_t new_ieee_link_id;
			uint32_t new_primary_freq;
			enum wlan_mlo_link_switch_reason reason;
		} link_switch;
		struct {
			uint32_t link_bitmap;
			uint8_t vdev_count;
			uint8_t mlo_vdev_lst[WLAN_UMAC_MLO_MAX_VDEVS];
			enum mlo_link_force_mode mode;
			enum mlo_link_force_reason reason;
		} tdls;
		struct {
			enum link_control_modes link_ctrl_mode;
			uint8_t link_num;
			uint32_t link_bitmap;
			uint32_t link_bitmap2;
			enum mlo_link_force_mode mode;
			enum mlo_link_force_reason reason;
		} vendor;
	} evt;
};

#ifdef WLAN_FEATURE_11BE_MLO
static inline const char *force_mode_to_string(uint32_t mode)
{
	switch (mode) {
	CASE_RETURN_STRING(MLO_LINK_FORCE_MODE_ACTIVE);
	CASE_RETURN_STRING(MLO_LINK_FORCE_MODE_INACTIVE);
	CASE_RETURN_STRING(MLO_LINK_FORCE_MODE_ACTIVE_NUM);
	CASE_RETURN_STRING(MLO_LINK_FORCE_MODE_INACTIVE_NUM);
	CASE_RETURN_STRING(MLO_LINK_FORCE_MODE_NO_FORCE);
	CASE_RETURN_STRING(MLO_LINK_FORCE_MODE_ACTIVE_INACTIVE);
	default:
		return "Unknown";
	}
};

#define ml_nlink_dump_force_state(_force_state, format, args...) \
	mlo_debug("inactive 0x%x active 0x%x inact num %d 0x%x act num %d 0x%x dyn 0x%x"format, \
			 (_force_state)->force_inactive_bitmap, \
			 (_force_state)->force_active_bitmap, \
			 (_force_state)->force_inactive_num, \
			 (_force_state)->force_inactive_num_bitmap, \
			 (_force_state)->force_active_num, \
			 (_force_state)->force_active_num_bitmap, \
			 (_force_state)->curr_dynamic_inactive_bitmap, \
			 ##args);

static inline const char *link_evt_to_string(uint32_t evt)
{
	switch (evt) {
	CASE_RETURN_STRING(ml_nlink_link_switch_start_evt);
	CASE_RETURN_STRING(ml_nlink_link_switch_pre_completion_evt);
	CASE_RETURN_STRING(ml_nlink_roam_sync_start_evt);
	CASE_RETURN_STRING(ml_nlink_roam_sync_completion_evt);
	CASE_RETURN_STRING(ml_nlink_connect_start_evt);
	CASE_RETURN_STRING(ml_nlink_connect_completion_evt);
	CASE_RETURN_STRING(ml_nlink_disconnect_start_evt);
	CASE_RETURN_STRING(ml_nlink_disconnect_completion_evt);
	CASE_RETURN_STRING(ml_nlink_ap_started_evt);
	CASE_RETURN_STRING(ml_nlink_ap_stopped_evt);
	CASE_RETURN_STRING(ml_nlink_connection_updated_evt);
	CASE_RETURN_STRING(ml_nlink_tdls_request_evt);
	CASE_RETURN_STRING(ml_nlink_vendor_cmd_request_evt);
	default:
		return "Unknown";
	}
};

/**
 * ml_nlink_conn_change_notify() - Notify connection state
 * change to force link module
 * @psoc: pointer to pdev
 * @vdev_id: vdev id
 * @evt: event type
 * @data: event data
 *
 * Return: QDF_STATUS_SUCCESS in the case of success
 */
QDF_STATUS
ml_nlink_conn_change_notify(struct wlan_objmgr_psoc *psoc,
			    uint8_t vdev_id,
			    enum ml_nlink_change_event_type evt,
			    struct ml_nlink_change_event *data);

#define MLO_MAX_VDEV_COUNT_PER_BIMTAP_ELEMENT (sizeof(uint32_t) * 8)

/**
 * ml_nlink_convert_linkid_bitmap_to_vdev_bitmap() - convert link
 * id bitmap to vdev id bitmap
 * state
 * @psoc: psoc object
 * @vdev: vdev object
 * @link_bitmap: link id bitmap
 * @associated_bitmap: all associated the link id bitmap
 * @vdev_id_bitmap_sz: number vdev id bitamp in vdev_id_bitmap
 * @vdev_id_bitmap: array to return the vdev id bitmaps
 * @vdev_id_num: total vdev id number in vdev_ids
 * @vdev_ids: vdev id array
 *
 * Return: None
 */
void
ml_nlink_convert_linkid_bitmap_to_vdev_bitmap(
			struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_vdev *vdev,
			uint32_t link_bitmap,
			uint32_t *associated_bitmap,
			uint32_t *vdev_id_bitmap_sz,
			uint32_t vdev_id_bitmap[MLO_VDEV_BITMAP_SZ],
			uint8_t *vdev_id_num,
			uint8_t vdev_ids[WLAN_MLO_MAX_VDEVS]);

/**
 * ml_nlink_convert_vdev_bitmap_to_linkid_bitmap() - convert vdev
 * id bitmap to link id bitmap
 * state
 * @psoc: psoc object
 * @vdev: vdev object
 * @vdev_id_bitmap_sz: array size of vdev_id_bitmap
 * @vdev_id_bitmap: vdev bitmap array
 * @link_bitmap: link id bitamp converted from vdev id bitmap
 * @associated_bitmap: all associated the link id bitmap
 *
 * Return: None
 */
void
ml_nlink_convert_vdev_bitmap_to_linkid_bitmap(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				uint32_t vdev_id_bitmap_sz,
				uint32_t *vdev_id_bitmap,
				uint32_t *link_bitmap,
				uint32_t *associated_bitmap);

/**
 * convert_link_bitmap_to_link_ids() - Convert link bitmap to link ids
 * @link_bitmap: PSOC object information
 * @link_id_sz: link_ids array size
 * @link_ids: link id array
 *
 * Return: num of link id in link_ids array converted from link bitmap
 */
uint32_t
convert_link_bitmap_to_link_ids(uint32_t link_bitmap,
				uint8_t link_id_sz,
				uint8_t *link_ids);

/**
 * ml_nlink_convert_link_bitmap_to_ids() - convert link bitmap
 * to link ids
 * @link_bitmap: link bitmap
 * @link_id_sz: array size of link_ids
 * @link_ids: link id array
 *
 * Return: number of link ids
 */
uint32_t
ml_nlink_convert_link_bitmap_to_ids(uint32_t link_bitmap,
				    uint8_t link_id_sz,
				    uint8_t *link_ids);

/**
 * enum set_curr_control - control flag to update current force bitmap
 * @LINK_OVERWRITE: use bitmap to overwrite existing force bitmap
 * @LINK_CLR: clr the input bitmap from existing force bitmap
 * @LINK_ADD: append the input bitmap to existing force bitamp
 *
 * This control value will be used in ml_nlink_set_* API to indicate
 * how to update input link_bitmap to current bitmap
 */
enum set_curr_control {
	LINK_OVERWRITE = 0x0,
	LINK_CLR       = 0x1,
	LINK_ADD       = 0x2,
};

/**
 * ml_nlink_set_curr_force_active_state() - set link force active
 * state
 * @psoc: psoc object
 * @vdev: vdev object
 * @link_bitmap: force active link id bitmap
 * @ctrl: control value to indicate how to update link_bitmap to current
 * bitmap
 *
 * Return: None
 */
void
ml_nlink_set_curr_force_active_state(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_vdev *vdev,
				     uint16_t link_bitmap,
				     enum set_curr_control ctrl);

/**
 * ml_nlink_set_curr_force_inactive_state() - set link force inactive
 * state
 * @psoc: psoc object
 * @vdev: vdev object
 * @link_bitmap: force inactive link id bitmap
 * @ctrl: control value to indicate how to update link_bitmap to current
 * bitmap
 *
 * Return: None
 */
void
ml_nlink_set_curr_force_inactive_state(struct wlan_objmgr_psoc *psoc,
				       struct wlan_objmgr_vdev *vdev,
				       uint16_t link_bitmap,
				       enum set_curr_control ctrl);

/**
 * ml_nlink_set_curr_force_active_num_state() - set link force active
 * number state
 * @psoc: psoc object
 * @vdev: vdev object
 * @link_num: force active num
 * @link_bitmap: force active num link id bitmap
 *
 * Return: None
 */
void
ml_nlink_set_curr_force_active_num_state(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_vdev *vdev,
					 uint8_t link_num,
					 uint16_t link_bitmap);

/**
 * ml_nlink_set_curr_force_inactive_num_state() - set link force inactive
 * number state
 * @psoc: psoc object
 * @vdev: vdev object
 * @link_num: force inactive num
 * @link_bitmap: force inactive num link id bitmap
 *
 * Return: None
 */
void
ml_nlink_set_curr_force_inactive_num_state(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_vdev *vdev,
					   uint8_t link_num,
					   uint16_t link_bitmap);

/**
 * ml_nlink_set_dynamic_inactive_links() - set link dynamic inactive
 * link bitmap
 * @psoc: psoc object
 * @vdev: vdev object
 * @dynamic_link_bitmap: dynamic inactive bitmap
 *
 * Return: None
 */
void
ml_nlink_set_dynamic_inactive_links(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_vdev *vdev,
				    uint16_t dynamic_link_bitmap);

/**
 * ml_nlink_get_dynamic_inactive_links() - get link dynamic inactive
 * link bitmap
 * @psoc: psoc object
 * @vdev: vdev object
 * @dynamic_link_bitmap: dynamic inactive bitmap
 * @force_link_bitmap: forced inactive bitmap
 *
 * Return: None
 */
void
ml_nlink_get_dynamic_inactive_links(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_vdev *vdev,
				    uint16_t *dynamic_link_bitmap,
				    uint16_t *force_link_bitmap);

/**
 * ml_nlink_init_concurrency_link_request() - Init concurrency force
 * link request
 * link bitmap
 * @psoc: psoc object
 * @vdev: vdev object
 *
 * When ML STA associated or Roam, initialize the concurrency
 * force link request based on "current" force link state
 *
 * Return: None
 */
void ml_nlink_init_concurrency_link_request(
	struct wlan_objmgr_psoc *psoc,
	struct wlan_objmgr_vdev *vdev);

/**
 * ml_nlink_get_force_link_request() - get link request of source
 * link bitmap
 * @psoc: psoc object
 * @vdev: vdev object
 * @req: set link request
 * @source: the source to query
 *
 * Return: None
 */
void
ml_nlink_get_force_link_request(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_vdev *vdev,
				struct set_link_req *req,
				enum set_link_source source);

/**
 * ml_nlink_get_curr_force_state() - get link force state
 * @psoc: psoc object
 * @vdev: vdev object
 * @force_cmd: current force state
 *
 * Return: None
 */
void
ml_nlink_get_curr_force_state(struct wlan_objmgr_psoc *psoc,
			      struct wlan_objmgr_vdev *vdev,
			      struct ml_link_force_state *force_cmd);

/**
 * ml_nlink_clr_force_state() - clear all link force state
 * @psoc: psoc object
 * @vdev: vdev object
 *
 * Return: None
 */
void
ml_nlink_clr_force_state(struct wlan_objmgr_psoc *psoc,
			 struct wlan_objmgr_vdev *vdev);

/**
 * ml_nlink_vendor_command_set_link() - Update vendor command
 * set link parameters
 * @psoc: psoc object
 * @vdev_id: vdev id
 * @link_control_mode: link control mode: default, user, mix
 * @reason: reason to set
 * @mode: mode to set
 * @link_num: number of link, valid for mode:
 * @link_bitmap: link bitmap, valid for mode:
 * @link_bitmap2: inactive link bitmap, only valid for mode
 *
 * Return: void
 */
void
ml_nlink_vendor_command_set_link(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id,
				 enum link_control_modes link_control_mode,
				 enum mlo_link_force_reason reason,
				 enum mlo_link_force_mode mode,
				 uint8_t link_num,
				 uint16_t link_bitmap,
				 uint16_t link_bitmap2);

/**
 * ml_is_nlink_service_supported() - support nlink or not
 * @psoc: psoc object
 *
 * Return: true if supported
 */
bool ml_is_nlink_service_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ml_nlink_get_standby_link_bitmap() - Get standby link info
 * @psoc: psoc
 * @vdev: vdev object
 *
 * Return: standby link bitmap
 */
uint32_t
ml_nlink_get_standby_link_bitmap(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev);
#else
static inline QDF_STATUS
ml_nlink_conn_change_notify(struct wlan_objmgr_psoc *psoc,
			    uint8_t vdev_id,
			    enum ml_nlink_change_event_type evt,
			    struct ml_nlink_change_event *data)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool
ml_is_nlink_service_supported(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif
#endif

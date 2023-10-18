/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: wlan_cp_stats_mc_ucfg_api.h
 *
 * This header file maintain API declaration required for northbound interaction
 */

#ifndef __WLAN_CP_STATS_MC_UCFG_API_H__
#define __WLAN_CP_STATS_MC_UCFG_API_H__

#ifdef QCA_SUPPORT_CP_STATS

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_cp_stats_mc_defs.h>

#ifdef WLAN_SUPPORT_TWT

#include <wlan_objmgr_peer_obj.h>
#include "../../core/src/wlan_cp_stats_defs.h"
#include <qdf_event.h>

/* Max TWT sessions supported */
#define TWT_PSOC_MAX_SESSIONS TWT_PEER_MAX_SESSIONS

/**
 * ucfg_twt_get_peer_session_params() - Retrieves peer twt session parameters
 * corresponding to a peer by using mac_addr and dialog id
 * If dialog_id is TWT_GET_ALL_PEER_PARAMS_DIALOG_ID retrieves twt session
 * parameters of all peers with valid twt session
 * @psoc_obj: psoc object
 * @param: array pointer to store peer twt session parameters, should contain
 * mac_addr and dialog id of a peer for which twt session stats to be retrieved
 *
 * Return: total number of valid twt session
 */
int
ucfg_twt_get_peer_session_params(struct wlan_objmgr_psoc *psoc_obj,
				 struct wmi_host_twt_session_stats_info *param);
#endif /* WLAN_SUPPORT_TWT */

struct psoc_cp_stats;
struct vdev_cp_stats;

/**
 * ucfg_mc_cp_stats_get_psoc_wake_lock_stats() : API to get wake lock stats from
 * psoc
 * @psoc: pointer to psoc object
 * @stats: stats object to populate
 *
 * Return : status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_get_psoc_wake_lock_stats(
						struct wlan_objmgr_psoc *psoc,
						struct wake_lock_stats *stats);

/**
 * ucfg_mc_cp_stats_get_vdev_wake_lock_stats() : API to get wake lock stats from
 * vdev
 * @vdev: pointer to vdev object
 * @stats: stats object to populate
 *
 * Return : status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_get_vdev_wake_lock_stats(
						struct wlan_objmgr_vdev *vdev,
						struct wake_lock_stats *stats);

/**
 * ucfg_mc_cp_stats_inc_wake_lock_stats_by_protocol() : API to increment wake
 * lock stats given the protocol of the packet that was received.
 * @psoc: pointer to psoc object
 * @vdev_id: vdev_id for which the packet was received
 * @protocol: protocol of the packet that was received
 *
 * Return : status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats_by_protocol(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					enum qdf_proto_subtype protocol);

/**
 * ucfg_mc_cp_stats_inc_wake_lock_stats_by_dst_addr() : API to increment wake
 * lock stats given destination of packet that was received.
 * @psoc: pointer to psoc object
 * @vdev_id: vdev_id for which the packet was received
 * @dest_mac: destination mac address of packet that was received
 *
 * Return : status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats_by_dst_addr(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, uint8_t *dest_mac);

/**
 * ucfg_mc_cp_stats_inc_wake_lock_stats() : API to increment wake lock stats
 * given wake reason.
 * @psoc: pointer to psoc object
 * @vdev_id: vdev_id on with WOW was received
 * @reason: reason of WOW
 *
 * Return : status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats(struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id,
						uint32_t reason);

/**
 * ucfg_mc_cp_stats_write_wow_stats() - Writes WOW stats to buffer
 * @psoc: pointer to psoc object
 * @buffer: The char buffer to write to
 * @max_len: The maximum number of chars to write
 * @ret: number of bytes written
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_write_wow_stats(
				struct wlan_objmgr_psoc *psoc,
				char *buffer, uint16_t max_len, int *ret);

/**
 * ucfg_mc_cp_stats_send_stats_request() - API to send stats request to lmac
 * @vdev: pointer to vdev object
 * @type: request type
 * @info: specific request information
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_send_stats_request(struct wlan_objmgr_vdev *vdev,
					       enum stats_req_type type,
					       struct request_info *info);

/**
 * wlan_cfg80211_mc_twt_clear_infra_cp_stats() - send request to reset
 * control path statistics
 * @vdev: pointer to vdev object
 * @dialog_id: dialod id of the twt session
 * @twt_peer_mac: mac address of the peer
 *
 * Return: 0 for success or error code for failure
 */
int
wlan_cfg80211_mc_twt_clear_infra_cp_stats(
				struct wlan_objmgr_vdev *vdev,
				uint32_t dialog_id,
				uint8_t twt_peer_mac[QDF_MAC_ADDR_SIZE]);

/**
 * wlan_cfg80211_mc_twt_get_infra_cp_stats() - send twt get statistic request
 * @vdev: pointer to vdev object
 * @dialog_id: TWT session dialog id
 * @twt_peer_mac: mac address of the peer
 * @errno: error code
 *
 * Return: pointer to infra cp stats event for success or NULL for failure
 */
struct infra_cp_stats_event *
wlan_cfg80211_mc_twt_get_infra_cp_stats(struct wlan_objmgr_vdev *vdev,
					uint32_t dialog_id,
					uint8_t twt_peer_mac[QDF_MAC_ADDR_SIZE],
					int *errno);
/**
 * ucfg_mc_cp_stats_get_tx_power() - API to fetch tx_power
 * @vdev: pointer to vdev object
 * @dbm: pointer to tx power in dbm
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_get_tx_power(struct wlan_objmgr_vdev *vdev,
					 int *dbm);

/**
 * ucfg_mc_cp_stats_is_req_pending() - API to tell if given request is pending
 * @psoc: pointer to psoc object
 * @type: request type to check
 *
 * Return: true of request is pending, false otherwise
 */
bool ucfg_mc_cp_stats_is_req_pending(struct wlan_objmgr_psoc *psoc,
				     enum stats_req_type type);

/**
 * ucfg_mc_cp_stats_set_pending_req() - API to set pending request
 * @psoc: pointer to psoc object
 * @type: request to update
 * @req: value to update
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_set_pending_req(struct wlan_objmgr_psoc *psoc,
					    enum stats_req_type type,
					    struct request_info *req);
/**
 * ucfg_mc_cp_stats_reset_pending_req() - API to reset pending request
 * @psoc: pointer to psoc object
 * @type: request to update
 * @last_req: last request
 * @pending: pending request present
 *
 * The function is an atomic operation of "reset" and "get" last request.
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_reset_pending_req(struct wlan_objmgr_psoc *psoc,
					      enum stats_req_type type,
					      struct request_info *last_req,
					      bool *pending);

/**
 * ucfg_mc_cp_stats_get_pending_req() - API to get pending request
 * @psoc: pointer to psoc object
 * @type: request to update
 * @info: buffer to populate
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_get_pending_req(struct wlan_objmgr_psoc *psoc,
					    enum stats_req_type type,
					    struct request_info *info);

/**
 * ucfg_mc_infra_cp_stats_free_stats_resources() - API to free buffers within
 * infra cp stats_event structure
 * @ev: structure whose buffer are to freed
 *
 * Return: none
 */
void
ucfg_mc_infra_cp_stats_free_stats_resources(struct infra_cp_stats_event *ev);

/**
 * ucfg_mc_cp_stats_free_stats_resources() - API to free buffers within stats_event
 * structure
 * @ev: structure whose buffer are to freed
 *
 * Return: none
 */
void ucfg_mc_cp_stats_free_stats_resources(struct stats_event *ev);

/**
 * ucfg_mc_cp_stats_cca_stats_get() - API to fetch cca stats
 * @vdev: pointer to vdev object
 * @cca_stats: pointer to cca info
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_cca_stats_get(struct wlan_objmgr_vdev *vdev,
					  struct cca_stats *cca_stats);

/**
 * ucfg_mc_cp_stats_set_rate_flags() - API to set rate flags
 * @vdev: pointer to vdev object
 * @flags: value to set (enum tx_rate_info)
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_mc_cp_stats_set_rate_flags(struct wlan_objmgr_vdev *vdev,
					   enum tx_rate_info flags);

/**
 * ucfg_mc_cp_stats_register_lost_link_info_cb() - API to register lost link
 * info callback
 * @psoc: pointer to psoc object
 * @lost_link_cp_stats_info_cb: Lost link info callback to be registered
 *
 */
void ucfg_mc_cp_stats_register_lost_link_info_cb(
		struct wlan_objmgr_psoc *psoc,
		void (*lost_link_cp_stats_info_cb)(void *stats_ev));

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD
/**
 * ucfg_mc_cp_stats_register_pmo_handler() - API to register pmo handler
 *
 * Return: none
 */
void ucfg_mc_cp_stats_register_pmo_handler(void);
#else
void static inline ucfg_mc_cp_stats_register_pmo_handler(void) { };
#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#ifdef WLAN_FEATURE_BIG_DATA_STATS
/**
 * ucfg_send_big_data_stats_request() - API to send big data stats
 * request
 * @vdev: pointer to vdev object
 * @type: request type
 * @info: request info
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_send_big_data_stats_request(struct wlan_objmgr_vdev *vdev,
					    enum stats_req_type type,
					    struct request_info *info);

/**
 * ucfg_mc_cp_set_big_data_fw_support() - set big data fw support
 * @psoc: PSOC object
 * @enable: Set true if firmware supports big data, otherwise false
 *
 * API to set fw supports big data feature or not
 *
 * Return: void
 */
void
ucfg_mc_cp_set_big_data_fw_support(struct wlan_objmgr_psoc *psoc,
				   bool enable);

/**
 * ucfg_mc_cp_get_big_data_fw_support() - get big data fw support
 * @psoc: PSOC object
 * @enable: Set true if firmware supports big data, otherwise false
 *
 * API to get fw supports big data feature or not
 *
 * Return: void
 */
void
ucfg_mc_cp_get_big_data_fw_support(struct wlan_objmgr_psoc *psoc,
				   bool *enable);
#else
static inline
QDF_STATUS ucfg_send_big_data_stats_request(struct wlan_objmgr_vdev *vdev,
					    enum stats_req_type type,
					    struct request_info *info)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_mc_cp_set_big_data_fw_support(struct wlan_objmgr_psoc *psoc,
				   bool enable)
{}

static inline void
ucfg_mc_cp_get_big_data_fw_support(struct wlan_objmgr_psoc *psoc,
				   bool *enable)
{}
#endif

#ifdef CONFIG_WLAN_BMISS
/**
 * wlan_cfg80211_mc_bmiss_get_infra_cp_stats() - API to get bmiss stats
 * @vdev: pointer to vdev object
 * @bmiss_peer_mac: mac address of the peer
 * @errno: error code
 *
 * Return: pointer to infra cp stats event for success or NULL for failure
 */
struct infra_cp_stats_event*
wlan_cfg80211_mc_bmiss_get_infra_cp_stats(
				struct wlan_objmgr_vdev *vdev,
				uint8_t bmiss_peer_mac[QDF_MAC_ADDR_SIZE],
				int *errno);
#else /* CONFIG_WLAN_BMISS */
static inline struct infra_cp_stats_event*
wlan_cfg80211_mc_bmiss_get_infra_cp_stats(
				struct wlan_objmgr_vdev *vdev,
				uint8_t bmiss_peer_mac[QDF_MAC_ADDR_SIZE],
				int *errno)
{
	return NULL;
}
#endif /* CONFIG_WLAN_BMISS */

/**
 * wlan_cp_stats_update_chan_info() - API to update chan stats
 * @psoc: pointer to psoc
 * @chan_stat: channel stats
 * @vdev_id: vdev id
 *
 * Return: None
 */
void wlan_cp_stats_update_chan_info(struct wlan_objmgr_psoc *psoc,
				    struct channel_status *chan_stat,
				    uint8_t vdev_id);

/**
 * ucfg_mc_cp_stats_clear_channel_status() - API to clear chan stats
 * @pdev: pointer to pdev object
 *
 * Return: None
 */
void ucfg_mc_cp_stats_clear_channel_status(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_mc_cp_stats_get_channel_status() - API to get chan stats
 * @pdev: pointer to pdev object
 * @chan_freq: channel freq of which stats are needed
 *
 * Return: channel status
 */
struct channel_status *
ucfg_mc_cp_stats_get_channel_status(struct wlan_objmgr_pdev *pdev,
				    uint32_t chan_freq);
#else /* QCA_SUPPORT_CP_STATS */

void static inline ucfg_mc_cp_stats_register_pmo_handler(void) { };
static inline QDF_STATUS ucfg_mc_cp_stats_send_stats_request(
				struct wlan_objmgr_vdev *vdev,
				enum stats_req_type type,
				struct request_info *info)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_mc_cp_stats_set_rate_flags(
				struct wlan_objmgr_vdev *vdev,
				enum tx_rate_info flags)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_mc_cp_stats_get_psoc_wake_lock_stats(
						struct wlan_objmgr_psoc *psoc,
						struct wake_lock_stats *stats)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats_by_protocol(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					enum qdf_proto_subtype protocol)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				uint32_t reason)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats_by_dst_addr(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, uint8_t *dest_mac)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_mc_cp_stats_get_vdev_wake_lock_stats(
						struct wlan_objmgr_vdev *vdev,
						struct wake_lock_stats *stats)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_send_big_data_stats_request(struct wlan_objmgr_vdev *vdev,
					    enum stats_req_type type,
					    struct request_info *info)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_mc_cp_set_big_data_fw_support(struct wlan_objmgr_psoc *psoc,
				   bool enable)
{}

static inline void
ucfg_mc_cp_big_data_fw_support(struct wlan_objmgr_psoc *psoc,
			       bool *enable)
{}

static inline struct infra_cp_stats_event*
wlan_cfg80211_mc_bmiss_get_infra_cp_stats(
				struct wlan_objmgr_vdev *vdev,
				uint8_t bmiss_peer_mac[QDF_MAC_ADDR_SIZE],
				int *errno)
{
	return NULL;
}

static inline void
ucfg_mc_cp_stats_get_tx_power(struct wlan_objmgr_vdev *vdev,
			      int *dbm)
{}

static inline
void wlan_cp_stats_update_chan_info(struct wlan_objmgr_psoc *psoc,
				    struct channel_status *chan_stat,
				    uint8_t vdev_id)
{
}

static inline
void ucfg_mc_cp_stats_clear_channel_status(struct wlan_objmgr_pdev *pdev)
{
}

static inline struct channel_status *
ucfg_mc_cp_stats_get_channel_status(struct wlan_objmgr_pdev *pdev,
				    uint32_t chan_freq)
{
	return NULL;
}
#endif /* QCA_SUPPORT_CP_STATS */
#endif /* __WLAN_CP_STATS_MC_UCFG_API_H__ */

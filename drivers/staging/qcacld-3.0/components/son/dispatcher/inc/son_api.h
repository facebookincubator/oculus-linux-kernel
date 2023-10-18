/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: contains interface prototypes for son api
 */
#ifndef _SON_API_H_
#define _SON_API_H_

#include <qdf_types.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <reg_services_public_struct.h>
#include <qdf_trace.h>

#define son_alert(format, args...) \
		QDF_TRACE_FATAL(QDF_MODULE_ID_SON, format, ## args)

#define son_err(format, args...) \
		QDF_TRACE_ERROR(QDF_MODULE_ID_SON, format, ## args)

#define son_warn(format, args...) \
		QDF_TRACE_WARN(QDF_MODULE_ID_SON, format, ## args)

#define son_info(format, args...) \
		QDF_TRACE_INFO(QDF_MODULE_ID_SON, format, ## args)

#define son_debug(format, args...) \
		QDF_TRACE_DEBUG(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_alert(format, args...) \
		QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_err(format, args...) \
		QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_warn(format, args...) \
		QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_info(format, args...) \
		QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_debug(format, args...) \
		QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_alert_rl(format, args...) \
		QDF_TRACE_FATAL_RL(QDF_MODULE_ID_SON, format, ## args)

#define son_err_rl(format, args...) \
		QDF_TRACE_ERROR_RL(QDF_MODULE_ID_SON, format, ## args)

#define son_warn_rl(format, args...) \
		QDF_TRACE_WARN_RL(QDF_MODULE_ID_SON, format, ## args)

#define son_info_rl(format, args...) \
		QDF_TRACE_INFO_RL(QDF_MODULE_ID_SON, format, ## args)

#define son_debug_rl(format, args...) \
		QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_alert_rl(format, args...) \
		QDF_TRACE_FATAL_RL_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_err_rl(format, args...) \
		QDF_TRACE_ERROR_RL_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_warn_rl(format, args...) \
		QDF_TRACE_WARN_RL_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_info_rl(format, args...) \
		QDF_TRACE_INFO_RL_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define son_nofl_debug_rl(format, args...) \
		QDF_TRACE_DEBUG_RL_NO_FL(QDF_MODULE_ID_SON, format, ## args)

#define TOTAL_DWELL_TIME 200

#define CBS_DEFAULT_RESTTIME 500 /* msec */
#define CBS_DEFAULT_DWELL_TIME 50 /* msec */
#define CBS_DEFAULT_WAIT_TIME 1000 /* 1 sec */
#define CBS_DEFAULT_DWELL_SPLIT_TIME 50 /* msec */
#define CBS_DEFAULT_DWELL_REST_TIME 500 /* msec */
#define CBS_DEFAULT_MIN_REST_TIME 50 /* msec */

#define DEFAULT_BEACON_INTERVAL 100

#define CBS_DWELL_TIME_10MS 10
#define CBS_DWELL_TIME_25MS 25
#define CBS_DWELL_TIME_50MS 50
#define CBS_DWELL_TIME_75MS 75
#define MIN_SCAN_OFFSET_ARRAY_SIZE 0
#define MAX_SCAN_OFFSET_ARRAY_SIZE 9
#define SCAN_START_OFFSET_MIN 26

#define DEFAULT_SCAN_MAX_REST_TIME 500

/**
 * enum son_cbs_state - son cbs state enumeration
 * @CBS_INIT: init state
 * @CBS_SCAN: scanning state
 * @CBS_REST: rest state
 * @CBS_RANK: rank state
 * @CBS_WAIT: wait state
 */
enum son_cbs_state {
	CBS_INIT,
	CBS_SCAN,
	CBS_REST,
	CBS_RANK,
	CBS_WAIT,
};

/**
 * struct son_cbs - son cbs struction
 * @vdev: vdev
 * @cbs_lock: cbs spin lock
 * @cbs_timer: cbs timer
 * @cbs_state: cbs state
 * @cbs_scan_requestor: scan requestor
 * @cbs_scan_id: scan id
 * @dwell_time: dwell time configuration
 * @rest_time: rest time configuration
 * @wait_time: wait time configuration
 * @scan_intvl_time: interval time configuration
 * @scan_params: scan params
 * @max_dwell_split_cnt: max dwell split counter
 * @dwell_split_cnt: dwell split counter
 * @scan_offset: scan offset array
 * @scan_dwell_rest: scan dwell rest array
 * @min_dwell_rest_time: nub dwell rest time
 * @dwell_split_time: dwell split time
 * @max_arr_size_used: max array size used
 */
struct son_cbs {
	struct wlan_objmgr_vdev *vdev;

	spinlock_t cbs_lock;
	qdf_timer_t cbs_timer;

	enum son_cbs_state cbs_state;

	wlan_scan_requester cbs_scan_requestor;
	wlan_scan_id cbs_scan_id;

	uint32_t dwell_time;
	uint32_t rest_time;
	uint32_t wait_time;
	int32_t  scan_intvl_time;

	struct scan_start_request scan_params;

	uint8_t max_dwell_split_cnt;
	int8_t dwell_split_cnt;
	uint32_t scan_offset[10];
	uint32_t scan_dwell_rest[10];
	uint32_t min_dwell_rest_time;
	uint32_t dwell_split_time;
	uint8_t max_arr_size_used;
};

/**
 * typedef mlme_deliver_cb() - cb to deliver mlme event
 * @vdev: pointer to vdev
 * @event_len: event length
 * @event_buf: event buffer
 *
 * Return: 0 if event is sent successfully
 */
typedef int (*mlme_deliver_cb)(struct wlan_objmgr_vdev *vdev,
			       uint32_t event_len,
			       const uint8_t *event_buf);

/**
 * enum SON_MLME_DELIVER_CB_TYPE - mlme deliver cb type
 * @SON_MLME_DELIVER_CB_TYPE_OPMODE: cb to deliver opmode
 * @SON_MLME_DELIVER_CB_TYPE_SMPS: cb to deliver smps
 */
enum SON_MLME_DELIVER_CB_TYPE {
	SON_MLME_DELIVER_CB_TYPE_OPMODE,
	SON_MLME_DELIVER_CB_TYPE_SMPS,
};

/**
 * wlan_son_register_mlme_deliver_cb - register mlme deliver cb
 * @psoc: pointer to psoc
 * @cb: mlme deliver cb
 * @type: mlme deliver cb type
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_son_register_mlme_deliver_cb(struct wlan_objmgr_psoc *psoc,
				  mlme_deliver_cb cb,
				  enum SON_MLME_DELIVER_CB_TYPE type);
/**
 * wlan_son_peer_ext_stat_enable() - sends EXT stats command to FW
 * @pdev: pointer to pdev
 * @mac_addr: MAC address of the target peer
 * @vdev: Pointer to vdev
 * @stats_count: Stats count
 * @enable: Enable / disable ext stats
 *
 * Return: QDF_STATUS_SUCCESS on success else failure
 */
QDF_STATUS wlan_son_peer_ext_stat_enable(struct wlan_objmgr_pdev *pdev,
					 uint8_t *mac_addr,
					 struct wlan_objmgr_vdev *vdev,
					 uint32_t stats_count,
					 uint32_t enable);

/**
 * wlan_son_peer_req_inst_stats() - Requests for instantaneous stats for
 *				    the target mac_addr from FW via
 *				    WMI_REQUEST_STATS_CMDID.
 * @pdev: pointer to pdev
 * @mac_addr: MAC address of the target peer
 * @vdev: Pointer to vdev
 *
 * Return: QDF_STATUS_SUCCESS on success else failure
 */
QDF_STATUS wlan_son_peer_req_inst_stats(struct wlan_objmgr_pdev *pdev,
					uint8_t *mac_addr,
					struct wlan_objmgr_vdev *vdev);

/**
 * wlan_son_get_chan_flag() - get chan flag
 * @pdev: pointer to pdev
 * @freq: qdf_freq_t
 * @flag_160: If true, 160 channel info will be obtained;
 *            otherwise 80+80, 80 channel info will be obtained
 * @chan_params: chan parameters
 *
 * Return: combination of enum qca_wlan_vendor_channel_prop_flags and
 *         enum qca_wlan_vendor_channel_prop_flags_2
 */
uint32_t wlan_son_get_chan_flag(struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq, bool flag_160,
				struct ch_params *chan_params);

/**
 * wlan_son_peer_set_kickout_allow() - set the peer is allowed to kickout
 * @vdev: pointer to vdev
 * @peer: pointer to peer
 * @kickout_allow: kickout_allow to set
 *
 * Return: QDF_STATUS_SUCCESS on Success else failure.
 */
QDF_STATUS wlan_son_peer_set_kickout_allow(struct wlan_objmgr_vdev *vdev,
					   struct wlan_objmgr_peer *peer,
					   bool kickout_allow);

/**
 * wlan_son_cbs_init() - son cbs init
 *
 * Return: 0 if succeed
 */
int wlan_son_cbs_init(void);

/* wlan_son_cbs_deinit - son cbs deinit
 *
 * Return: 0 if succeed
 */
int wlan_son_cbs_deinit(void);

/* wlan_son_cbs_enable() - son cbs enable
 * @vdev: pointer to vdev
 *
 * Return: 0 if succeed
 */
int wlan_son_cbs_enable(struct wlan_objmgr_vdev *vdev);

/* wlan_son_cbs_disable() - son cbs disable
 * @vdev: pointer to vdev
 *
 * Return: 0 if succeed
 */
int wlan_son_cbs_disable(struct wlan_objmgr_vdev *vdev);

/* wlan_son_set_cbs() - son cbs set
 * @vdev: pointer to vdev
 * @enable: enable or disable son cbs
 *
 * Return: 0 if succeed
 */
int wlan_son_set_cbs(struct wlan_objmgr_vdev *vdev,
		     bool enable);

/* wlan_son_set_cbs_wait_time() - cbs wait time configure
 * @vdev: pointer to vdev
 * @val: wait time value
 *
 * Return: 0 if succeed
 */
int wlan_son_set_cbs_wait_time(struct wlan_objmgr_vdev *vdev,
			       uint32_t val);

/* wlan_son_set_cbs_dwell_split_time() - cbs dwell spilt time configure
 * @vdev: pointer to vdev
 * @val: dwell spilt time value
 *
 * Return: 0 if succeed
 */
int wlan_son_set_cbs_dwell_split_time(struct wlan_objmgr_vdev *vdev,
				      uint32_t val);

/**
 * wlan_son_vdev_get_supported_txrx_streams() - get supported spatial streams
 * @vdev: pointer to vdev
 * @num_tx_streams: pointer to number of tx streams
 * @num_rx_streams: pointer to number of rx streams
 *
 * Return: QDF_STATUS_SUCCESS on Success else failure.
 */
QDF_STATUS
wlan_son_vdev_get_supported_txrx_streams(struct wlan_objmgr_vdev *vdev,
					 uint32_t *num_tx_streams,
					 uint32_t *num_rx_streams);

#ifdef WLAN_FEATURE_SON
/**
 * wlan_son_peer_is_kickout_allow() - Is peer is allowed to kickout
 * @vdev: pointer to vdev
 * @macaddr: mac addr of the peer
 *
 * Return: True if it is allowed to kickout.
 */
bool wlan_son_peer_is_kickout_allow(struct wlan_objmgr_vdev *vdev,
				    uint8_t *macaddr);

/**
 * wlan_son_ind_assoc_req_frm() - indicate assoc req frame to son
 * @vdev: pointer to vdev
 * @macaddr: MAC address
 * @is_reassoc: true if it is reassoc req
 * @frame: frame body
 * @frame_len: frame body length
 * @status: assoc req frame is handled successfully
 *
 * Return: Void
 */
void wlan_son_ind_assoc_req_frm(struct wlan_objmgr_vdev *vdev,
				uint8_t *macaddr, bool is_reassoc,
				uint8_t *frame, uint16_t frame_len,
				QDF_STATUS status);

/**
 * wlan_son_deliver_tx_power() - notify son module of tx power
 * @vdev: vdev
 * @max_pwr: max power in dBm unit
 *
 * Return: 0 if event is sent successfully
 */
int wlan_son_deliver_tx_power(struct wlan_objmgr_vdev *vdev,
			      int32_t max_pwr);

/**
 * wlan_son_deliver_vdev_stop() - notify son module of vdev stop
 * @vdev: vdev
 *
 * Return: 0 if event is sent successfully
 */
int wlan_son_deliver_vdev_stop(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_son_deliver_inst_rssi() - notify son module of inst rssi
 * @vdev: vdev
 * @peer: peer device
 * @irssi: inst rssi above the noise floor in dB unit
 *
 * Return: 0 if event is sent successfully
 */
int wlan_son_deliver_inst_rssi(struct wlan_objmgr_vdev *vdev,
			       struct wlan_objmgr_peer *peer,
			       uint32_t irssi);

/**
 * wlan_son_deliver_opmode() - notify user app of opmode
 * @vdev: vdev objmgr
 * @bw: channel width defined in enum eSirMacHTChannelWidth
 * @nss: supported rx nss
 * @addr: source addr
 *
 * Return: 0 if event is sent successfully
 */
int wlan_son_deliver_opmode(struct wlan_objmgr_vdev *vdev,
			    uint8_t bw,
			    uint8_t nss,
			    uint8_t *addr);

/**
 * wlan_son_deliver_smps() - notify user app of smps
 * @vdev: vdev objmgr
 * @is_static: is_static
 * @addr: source addr
 *
 * Return: 0 if event is sent successfully
 */
int wlan_son_deliver_smps(struct wlan_objmgr_vdev *vdev,
			  uint8_t is_static,
			  uint8_t *addr);

/**
 * wlan_son_deliver_rrm_rpt() - notify son module of rrm rpt
 * @vdev: vdev objmgr
 * @addr: sender addr
 * @frm: points to measurement report
 * @flen: frame length
 *
 * Return: 0 if event is sent successfully
 */
int wlan_son_deliver_rrm_rpt(struct wlan_objmgr_vdev *vdev,
			     uint8_t *addr,
			     uint8_t *frm,
			     uint32_t flen);
/**
 * wlan_son_anqp_frame() - notify son module of mgmt frames
 * @vdev: vdev
 * @subtype: frame subtype
 * @frame: the 802.11 frame
 * @frame_len: frame length
 * @action_hdr: Action header of the frame
 * @macaddr: source mac address
 *
 * Return: 0 if event is sent successfully
 */
int wlan_son_anqp_frame(struct wlan_objmgr_vdev *vdev, int subtype,
			uint8_t *frame, uint16_t frame_len, void *action_hdr,
			uint8_t *macaddr);

/**
 * wlan_son_get_node_tx_power() - Gets the max transmit power for peer
 * @assoc_req_ies: assoc req ies
 *
 * Return: Returns the max tx power
 */
uint8_t wlan_son_get_node_tx_power(struct element_info assoc_req_ies);

/**
 * wlan_son_get_peer_rrm_info() - Get RRM info for peer
 * @assoc_req_ies: assoc req ies
 * @rrmcaps: rrm capabilities
 * @is_beacon_meas_supported: if beacon meas is supported
 *
 * Return: Returns QDF_STATUS_SUCCESS if succeed
 */
QDF_STATUS wlan_son_get_peer_rrm_info(struct element_info assoc_req_ies,
				      uint8_t *rrmcaps,
				      bool *is_beacon_meas_supported);
#else

static inline bool wlan_son_peer_is_kickout_allow(struct wlan_objmgr_vdev *vdev,
						  uint8_t *macaddr)
{
	return true;
}

static inline
void wlan_son_ind_assoc_req_frm(struct wlan_objmgr_vdev *vdev,
				uint8_t *macaddr, bool is_reassoc,
				uint8_t *frame, uint16_t frame_len,
				QDF_STATUS status)
{
}

static inline
int wlan_son_deliver_tx_power(struct wlan_objmgr_vdev *vdev,
			      int32_t max_pwr)
{
	return -EINVAL;
}

static inline
int wlan_son_deliver_vdev_stop(struct wlan_objmgr_vdev *vdev)
{
	return -EINVAL;
}

static inline
int wlan_son_deliver_inst_rssi(struct wlan_objmgr_vdev *vdev,
			       struct wlan_objmgr_peer *peer,
			       uint32_t irssi)
{
	return -EINVAL;
}

static inline
int wlan_son_deliver_opmode(struct wlan_objmgr_vdev *vdev,
			    uint8_t bw,
			    uint8_t nss,
			    uint8_t *addr)
{
	return -EINVAL;
}

static inline
int wlan_son_deliver_smps(struct wlan_objmgr_vdev *vdev,
			  uint8_t is_static,
			  uint8_t *addr)
{
	return -EINVAL;
}

static inline
int wlan_son_deliver_rrm_rpt(struct wlan_objmgr_vdev *vdev,
			     uint8_t *mac_addr,
			     uint8_t *frm,
			     uint32_t flen)
{
	return -EINVAL;
}

static inline
int wlan_son_anqp_frame(struct wlan_objmgr_vdev *vdev, int subtype,
			uint8_t *frame, uint16_t frame_len, void *action_hdr,
			uint8_t *macaddr)
{
	return -EINVAL;
}

static inline
uint8_t wlan_son_get_node_tx_power(struct element_info assoc_req_ies)
{
	return 0;
}

static inline
QDF_STATUS wlan_son_get_peer_rrm_info(struct element_info assoc_req_ies,
				      uint8_t *rrmcaps,
				      bool *is_beacon_meas_supported)
{
	return QDF_STATUS_E_INVAL;
}
#endif /*WLAN_FEATURE_SON*/
#endif

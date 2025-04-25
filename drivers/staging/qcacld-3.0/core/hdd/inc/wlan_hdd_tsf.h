/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#if !defined WLAN_HDD_TSF_H
#define WLAN_HDD_TSF_H
#include "wlan_hdd_cfg.h"
#include "wlan_hdd_main.h"
#ifdef WLAN_FEATURE_TSF_ACCURACY
#include "qdf_hrtimer.h"
#endif
/**
 * enum hdd_tsf_get_state - status of get tsf action
 * @TSF_RETURN:                   get tsf
 * @TSF_STA_NOT_CONNECTED_NO_TSF: sta not connected to ap
 * @TSF_NOT_RETURNED_BY_FW:       fw not returned tsf
 * @TSF_CURRENT_IN_CAP_STATE:     driver in capture state
 * @TSF_CAPTURE_FAIL:             capture fail
 * @TSF_GET_FAIL:                 get fail
 * @TSF_RESET_GPIO_FAIL:          GPIO reset fail
 * @TSF_SAP_NOT_STARTED_NO_TSF:   SAP not started
 * @TSF_NOT_READY: TSF module is not initialized or init failed
 * @TSF_DISABLED_BY_TSFPLUS: cap_tsf/get_tsf are disabled due to TSF_PLUS
 */
enum hdd_tsf_get_state {
	TSF_RETURN = 0,
	TSF_STA_NOT_CONNECTED_NO_TSF,
	TSF_NOT_RETURNED_BY_FW,
	TSF_CURRENT_IN_CAP_STATE,
	TSF_CAPTURE_FAIL,
	TSF_GET_FAIL,
	TSF_RESET_GPIO_FAIL,
	TSF_SAP_NOT_STARTED_NO_TSF,
	TSF_NOT_READY,
	TSF_DISABLED_BY_TSFPLUS
};

/**
 * enum hdd_tsf_capture_state - status of capture
 * @TSF_IDLE:      idle
 * @TSF_CAP_STATE: current is in capture state
 */
enum hdd_tsf_capture_state {
	TSF_IDLE = 0,
	TSF_CAP_STATE
};

/**
 * struct hdd_tsf_op_response - Store TSF sync parameters if TSF sync is active
 * @status: TSF response status defined by enum hdd_tsf_get_state
 * @time: TSF sync Target time. Time unit is microseconds.
 * @soc_time: TSF sync SOC time. Time unit is microseconds.
 */
struct hdd_tsf_op_response {
	enum hdd_tsf_get_state status;
	uint64_t time;
	uint64_t soc_time;
};

/**
 * enum hdd_tsf_auto_rpt_source - trigger source of tsf auto report
 * @HDD_TSF_AUTO_RPT_SOURCE_UPLINK_DELAY: uplink delay feature
 * @HDD_TSF_AUTO_RPT_SOURCE_TX_LATENCY: transmit latency statistics
 */
enum hdd_tsf_auto_rpt_source {
	HDD_TSF_AUTO_RPT_SOURCE_UPLINK_DELAY,
	HDD_TSF_AUTO_RPT_SOURCE_TX_LATENCY,
};

#ifdef WLAN_FEATURE_TSF_AUTO_REPORT
/**
 * hdd_set_tsf_auto_report() - enable or disable tsf auto report
 * for an adapter
 * @adapter: pointer to Adapter context
 * @ena: requesting state (true or false)
 * @source: source of the request
 *
 * Return: 0 for success or non-zero negative failure code
 */
int
hdd_set_tsf_auto_report(struct hdd_adapter *adapter, bool ena,
			enum hdd_tsf_auto_rpt_source source);

/**
 * hdd_tsf_auto_report_init() - initialize tsf auto report related
 * structures for an adapter
 * @adapter: pointer to Adapter context
 *
 * Return: None
 */
void hdd_tsf_auto_report_init(struct hdd_adapter *adapter);
#else
static inline int
hdd_set_tsf_auto_report(struct hdd_adapter *adapter, bool ena,
			enum hdd_tsf_auto_rpt_source source)
{
	return -ENOTSUPP;
}

static inline void hdd_tsf_auto_report_init(struct hdd_adapter *adapter)
{
}
#endif

/**
 * struct hdd_vdev_tsf - Adapter level tsf params
 * @cur_target_time: tsf value received from firmware.
 * @cur_tsf_sync_soc_time: Current SOC time.
 * @last_tsf_sync_soc_time: Last SOC time when TSF was synced.
 * @cur_target_global_tsf_time: Global Fw TSF time.
 * @last_target_global_tsf_time: Last reported global Fw TSF time.
 * @host_capture_req_timer: Host timer to capture TSF time.
 * @tsf_id: TSF id as obtained from FW report.
 * @tsf_mac_id: mac_id as obtained from FW report.
 * @tsf_details_valid: flag indicating whether tsf details are valid.
 * @host_target_sync_lock: spin lock for read/write timestamps.
 * @host_target_sync_timer: Timer to Sync host target.
 * @host_trigger_gpio_timer: A hrtimer used for TSF Accuracy Feature to
 *                           indicate TSF cycle complete.
 * @enable_dynamic_tsf_sync: Enable/Disable TSF sync through NL interface.
 * @host_target_sync_force: Force update host to TSF mapping.
 * @dynamic_tsf_sync_interval: TSF sync interval configure through NL interface.
 * @cur_host_time: Current Host time.
 * @last_host_time: Host time when TSF read was done.
 * @last_target_time: Last Fw reported time when TSF read was done.
 * @continuous_error_count: Store the count of continuous invalid tstamp-pair.
 * @continuous_cap_retry_count: to store the count of continuous capture retry.
 * @tsf_sync_ready_flag: to indicate whether tsf_sync has been initialized.
 * @gpio_tsf_sync_work: work to sync send TSF CAP WMI command.
 * @auto_rpt_src: bitmap to record trigger sources of TSF auto report
 */
struct hdd_vdev_tsf {
	uint64_t cur_target_time;
	uint64_t cur_tsf_sync_soc_time;
	uint64_t last_tsf_sync_soc_time;
	uint64_t cur_target_global_tsf_time;
	uint64_t last_target_global_tsf_time;
	qdf_mc_timer_t host_capture_req_timer;
#ifdef QCA_GET_TSF_VIA_REG
	int tsf_id;
	int tsf_mac_id;
	qdf_atomic_t tsf_details_valid;
#endif
#ifdef WLAN_FEATURE_TSF_PLUS
	qdf_spinlock_t host_target_sync_lock;
	qdf_mc_timer_t host_target_sync_timer;
#ifdef WLAN_FEATURE_TSF_ACCURACY
	qdf_hrtimer_data_t host_trigger_gpio_timer;
#endif
	bool enable_dynamic_tsf_sync;
	bool host_target_sync_force;
	uint32_t dynamic_tsf_sync_interval;
	uint64_t cur_host_time;
	uint64_t last_host_time;
	uint64_t last_target_time;
	int continuous_error_count;
	int continuous_cap_retry_count;
	qdf_atomic_t tsf_sync_ready_flag;
#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
	qdf_work_t gpio_tsf_sync_work;
#endif
#endif /* WLAN_FEATURE_TSF_PLUS */
#ifdef WLAN_FEATURE_TSF_AUTO_REPORT
	unsigned long auto_rpt_src;

#endif /* WLAN_FEATURE_TSF_UPLINK_DELAY */
};

/**
 * struct hdd_ctx_tsf - Context level tsf params
 * @tsf_ready_flag: indicate whether tsf has been initialized.
 * @cap_tsf_flag: indicate whether it's now capturing tsf(updating tstamp-pair).
 * @cap_tsf_context:  the context that is capturing tsf.
 * @tsf_accuracy_context: the context that is capturing tsf accuracy.
 * @ptp_cinfo: TSF PTP clock info.
 * @ptp_clock: TSF PTP clock.
 */
struct hdd_ctx_tsf {
	qdf_atomic_t tsf_ready_flag;
	qdf_atomic_t cap_tsf_flag;
	struct hdd_adapter *cap_tsf_context;
#ifdef WLAN_FEATURE_TSF_ACCURACY
	struct hdd_adapter *tsf_accuracy_context;
#endif
#ifdef WLAN_FEATURE_TSF_PTP
	struct ptp_clock_info ptp_cinfo;
	struct ptp_clock *ptp_clock;
#endif
};

#ifdef WLAN_FEATURE_TSF_UPLINK_DELAY
/**
 * hdd_get_uplink_delay_len() - get uplink delay length
 * @adapter: pointer to the adapter
 *
 * Return: uplink delay length
 */
uint32_t hdd_get_uplink_delay_len(struct hdd_adapter *adapter);

/**
 * hdd_add_uplink_delay() - add uplink delay
 * @adapter: pointer to the adapter
 * @skb: nbuf
 *
 * Return: status
 */
QDF_STATUS hdd_add_uplink_delay(struct hdd_adapter *adapter,
				struct sk_buff *skb);
#else /* !WLAN_FEATURE_TSF_UPLINK_DELAY */
static inline uint32_t hdd_get_uplink_delay_len(struct hdd_adapter *adapter)
{
	return 0;
}

static inline QDF_STATUS hdd_add_uplink_delay(struct hdd_adapter *adapter,
					      struct sk_buff *skb)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_TSF_UPLINK_DELAY */
#ifdef WLAN_FEATURE_TSF
/**
 * wlan_hdd_tsf_init() - set gpio and callbacks for
 *     capturing tsf and init tsf_plus
 * @hdd_ctx: pointer to the struct hdd_context
 *
 * This function set the callback to sme module, the callback will be
 * called when a tsf event is reported by firmware; set gpio number
 * to FW, FW will toggle this gpio when received a CAP_TSF command;
 * do tsf_plus init
 *
 * Return: nothing
 */
void wlan_hdd_tsf_init(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_tsf_deinit() - reset callbacks for capturing tsf, deinit tsf_plus
 * @hdd_ctx: pointer to the struct hdd_context
 *
 * This function reset the callback to sme module, and deinit tsf_plus
 *
 * Return: nothing
 */
void wlan_hdd_tsf_deinit(struct hdd_context *hdd_ctx);

/**
 * hdd_capture_tsf() - capture tsf
 * @adapter: pointer to adapter
 * @buf: pointer to uplayer buf
 * @len : the length of buf
 *
 * This function returns tsf value to uplayer.
 *
 * Return: 0 for success or non-zero negative failure code
 */
int hdd_capture_tsf(struct hdd_adapter *adapter, uint32_t *buf, int len);

/**
 * hdd_indicate_tsf() - return tsf to uplayer
 *
 * @adapter: pointer to adapter
 * @tsf_op_resp: pointer to struct hdd_tsf_op_response
 *
 * This function returns tsf value to uplayer.
 *
 * Return: Describe the execute result of this routine
 */
int hdd_indicate_tsf(struct hdd_adapter *adapter,
		     struct hdd_tsf_op_response *tsf_op_resp);

/**
 * wlan_hdd_cfg80211_handle_tsf_cmd(): Setup TSF operations
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Handle TSF SET / GET operation from userspace
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_handle_tsf_cmd(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len);

int hdd_get_tsf_cb(void *pcb_cxt, struct stsf *ptsf);

extern const struct nla_policy tsf_policy[QCA_WLAN_VENDOR_ATTR_TSF_MAX + 1];

#define FEATURE_HANDLE_TSF_VENDOR_COMMANDS \
{ \
	.info.vendor_id = QCA_NL80211_VENDOR_ID, \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TSF, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
		WIPHY_VENDOR_CMD_NEED_NETDEV | \
		WIPHY_VENDOR_CMD_NEED_RUNNING, \
	.doit = wlan_hdd_cfg80211_handle_tsf_cmd, \
	vendor_command_policy(tsf_policy, QCA_WLAN_VENDOR_ATTR_TSF_MAX)\
},
#else
static inline void wlan_hdd_tsf_init(struct hdd_context *hdd_ctx)
{
}

static inline void wlan_hdd_tsf_deinit(struct hdd_context *hdd_ctx)
{
}

static inline int hdd_indicate_tsf(struct hdd_adapter *adapter,
				   struct hdd_tsf_op_response *tsf_op_resp)
{
	return -ENOTSUPP;
}

static inline int
hdd_capture_tsf(struct hdd_adapter *adapter, uint32_t *buf, int len)
{
	return -ENOTSUPP;
}

static inline int wlan_hdd_cfg80211_handle_tsf_cmd(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	return -ENOTSUPP;
}
static inline int hdd_get_tsf_cb(void *pcb_cxt, struct stsf *ptsf)
{
	return -ENOTSUPP;
}

#define FEATURE_HANDLE_TSF_VENDOR_COMMANDS
#endif

#if defined(WLAN_FEATURE_TSF_PLUS) && defined(WLAN_FEATURE_TSF)
/**
 * hdd_tsf_is_tx_set() - check ini configuration
 * @hdd: pointer to hdd context
 *
 * This function checks tsf configuration for ptp on tx
 *
 * Return: true on enable, false on disable
 */

bool hdd_tsf_is_tx_set(struct hdd_context *hdd);
/**
 * hdd_tsf_is_rx_set() - check ini configuration
 * @hdd: pointer to hdd context
 *
 * This function checks tsf configuration for ptp on rx
 *
 * Return: true on enable, false on disable
 */
bool hdd_tsf_is_rx_set(struct hdd_context *hdd);
/**
 * hdd_tsf_is_raw_set() - check ini configuration
 * @hdd: pointer to hdd context
 *
 * This function checks tsf configuration for ptp on raw
 *
 * Return: true on enable, false on disable
 */
bool hdd_tsf_is_raw_set(struct hdd_context *hdd);
/**
 * hdd_tsf_is_dbg_fs_set() - check ini configuration
 * @hdd: pointer to hdd context
 *
 * This function checks tsf configuration for ptp on dbg fs
 *
 * Return: true on enable, false on disable
 */
bool hdd_tsf_is_dbg_fs_set(struct hdd_context *hdd);

/**
 * hdd_start_tsf_sync() - start tsf sync
 * @adapter: pointer to adapter
 *
 * This function initialize and start TSF synchronization
 *
 * Return: Describe the execute result of this routine
 */
int hdd_start_tsf_sync(struct hdd_adapter *adapter);

/**
 * hdd_restart_tsf_sync_post_wlan_resume() - restart host TSF sync
 * @adapter: pointer to adapter
 *
 * This function restarts host TSF sync immediately after wlan resume
 *
 * Return: none
 */
void hdd_restart_tsf_sync_post_wlan_resume(struct hdd_adapter *adapter);

/**
 * hdd_stop_tsf_sync() - stop tsf sync
 * @adapter: pointer to adapter
 *
 * This function stop and de-initialize TSF synchronization
 *
 * Return: Describe the execute result of this routine
 */
int hdd_stop_tsf_sync(struct hdd_adapter *adapter);

/**
 * hdd_capture_req_timer_expired_handler() - capture req timer handler
 * @arg: pointer to a adapter
 *
 * This function set a timeout handler for TSF capture timer.
 *
 * Return: none
 */

void hdd_capture_req_timer_expired_handler(void *arg);

/**
 * hdd_tsf_is_tsf64_tx_set() - check ini configuration
 * @hdd: pointer to hdd context
 *
 * This function checks tsf configuration for ptp on tsf64 tx
 *
 * Return: true on enable, false on disable
 */
bool hdd_tsf_is_tsf64_tx_set(struct hdd_context *hdd);

/**
 * hdd_tsf_is_time_sync_enabled_cfg() - check ini configuration
 * @hdd_ctx: pointer to hdd context
 *
 * This function checks tsf configuration for ptp for tsf
 * sync period
 * Return: true on enable, false on disable
 */
bool hdd_tsf_is_time_sync_enabled_cfg(struct hdd_context *hdd_ctx);

/**
 * hdd_update_dynamic_tsf_sync - Configure TSF mode for vdev
 * @adapter: pointer to hdd adapter
 *
 * This function configures TSF mode for vdev with ini parameter
 */
void hdd_update_dynamic_tsf_sync(struct hdd_adapter *adapter);

#ifdef WLAN_FEATURE_TSF_PLUS_SOCK_TS
/**
 * hdd_rx_timestamp() - time stamp RX netbuf
 *
 * @netbuf: pointer to a RX netbuf
 * @target_time: RX time for the netbuf
 *
 * This function get corresponding host time from target time,
 * and time stamp the RX netbuf with this time
 *
 * Return: Describe the execute result of this routine
 */
int hdd_rx_timestamp(qdf_nbuf_t netbuf, uint64_t target_time);
#endif

/**
 * hdd_get_tsf_time() - get tsf time for system time
 *
 * @adapter_ctx: adapter context
 * @input_time: input system time
 * @tsf_time: tsf time for system time
 *
 * Return: qdf status
 */
QDF_STATUS hdd_get_tsf_time(void *adapter_ctx, uint64_t input_time,
			    uint64_t *tsf_time);
#else
static inline int hdd_start_tsf_sync(struct hdd_adapter *adapter)
{
	return -ENOTSUPP;
}

static inline int hdd_stop_tsf_sync(struct hdd_adapter *adapter)
{
	return -ENOTSUPP;
}

static inline
void hdd_capture_req_timer_expired_handler(void *arg)
{
}

static inline
bool hdd_tsf_is_tsf64_tx_set(struct hdd_context *hdd)
{
	return FALSE;
}

static inline
bool hdd_tsf_is_time_sync_enabled_cfg(struct hdd_context *hdd_ctx)
{
	return false;
}

static inline
void hdd_update_dynamic_tsf_sync(struct hdd_adapter *adapter)
{
}

static inline
void hdd_restart_tsf_sync_post_wlan_resume(struct hdd_adapter *adapter)
{
}

static inline
QDF_STATUS hdd_get_tsf_time(void *adapter_ctx, uint64_t input_time,
			    uint64_t *tsf_time)
{
	*tsf_time = 0;
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#ifdef WLAN_FEATURE_TSF_PTP
/**
 * wlan_get_ts_info() - return ts info to uplayer
 * @dev: pointer to net_device
 * @info: pointer to ethtool_ts_info
 *
 * Return: Describe the execute result of this routine
 */
int wlan_get_ts_info(struct net_device *dev, struct ethtool_ts_info *info);

#endif
#endif

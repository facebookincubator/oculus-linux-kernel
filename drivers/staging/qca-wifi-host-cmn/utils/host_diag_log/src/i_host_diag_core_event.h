/*
 * Copyright (c) 2014-2019, 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#if !defined(__I_HOST_DIAG_CORE_EVENT_H)
#define __I_HOST_DIAG_CORE_EVENT_H

/**=========================================================================

   \file  i_host_diag_core_event.h

   \brief Android specific definitions for WLAN UTIL DIAG events

   ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include <qdf_types.h>
#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include <host_diag_event_defs.h>
#endif

/*--------------------------------------------------------------------------
   Preprocessor definitions and constants
   ------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef FEATURE_WLAN_DIAG_SUPPORT

void host_diag_event_report_payload(uint16_t event_Id, uint16_t length,
				    void *pPayload);
/*---------------------------------------------------------------------------
   Allocate an event payload holder
   ---------------------------------------------------------------------------*/
#define WLAN_HOST_DIAG_EVENT_DEF(payload_name, payload_type) \
	payload_type(payload_name)

/*---------------------------------------------------------------------------
   Report the event
   ---------------------------------------------------------------------------*/
#define WLAN_HOST_DIAG_EVENT_REPORT(payload_ptr, ev_id) \
	do {							\
		host_diag_event_report_payload(ev_id,		\
						sizeof(*(payload_ptr)),			\
						(void *)(payload_ptr));			\
	} while (0)

#else                           /* FEATURE_WLAN_DIAG_SUPPORT */

#define WLAN_HOST_DIAG_EVENT_DEF(payload_name, payload_type)
#define WLAN_HOST_DIAG_EVENT_REPORT(payload_ptr, ev_id)

#endif /* FEATURE_WLAN_DIAG_SUPPORT */

/**
 * enum auth_timeout_type - authentication timeout type
 * @AUTH_FAILURE_TIMEOUT: auth failure timeout
 * @AUTH_RESPONSE_TIMEOUT: auth response timeout
 */
enum auth_timeout_type {
	AUTH_FAILURE_TIMEOUT,
	AUTH_RESPONSE_TIMEOUT,
};

#ifdef CONNECTIVITY_DIAG_EVENT
/**
 * enum diag_roam_reason - Represents the reason codes for roaming.
 * @DIAG_ROAM_REASON_UNKNOWN: Any reason that do not classify under the below
 * reasons.
 * @DIAG_ROAM_REASON_PER: Roam triggered when packet error rates(PER) breached
 * the configured threshold.
 * @DIAG_ROAM_REASON_BEACON_MISS: Roam triggered due to the continuous
 * configured beacon misses from the then connected AP.
 * @DIAG_ROAM_REASON_POOR_RSSI: Roam triggered due to the poor RSSI reported
 * by the connected AP.
 * @DIAG_ROAM_REASON_BETTER_RSSI: Roam triggered for finding a BSSID with a
 * better RSSI than the connected BSSID. Here the RSSI of the current BSSID is
 * not poor.
 * @DIAG_ROAM_REASON_CONGESTION: Roam triggered considering the connected
 * channel or environment being very noisy / congested.
 * @DIAG_ROAM_REASON_EXPLICIT_REQUEST: Roam triggered due to an explicit request
 * from the user (user space).
 * @DIAG_ROAM_REASON_BTM: Roam triggered due to BTM request frame received from
 * connected AP.
 * @DIAG_ROAM_REASON_BSS_LOAD: Roam triggered due to the channel utilization
 * breaching out the configured threshold.
 * @DIAG_ROAM_REASON_WTC: Roam triggered due to Wireless to Cellular BSS
 * transition request.
 * @DIAG_ROAM_REASON_IDLE: Roam triggered when device is suspended,
 * there is no data activity with the AP and the current rssi falls below a
 * certain threshold.
 * @DIAG_ROAM_REASON_DISCONNECTION: Roam triggered due to
 * deauthentication or disassociation frames received from the connected AP.
 * @DIAG_ROAM_REASON_PERIODIC_TIMER: Roam triggered as part of the periodic
 * scan that happens when there is no candidate AP found during the poor
 * RSSI scan trigger.
 * @DIAG_ROAM_REASON_BACKGROUND_SCAN: Roam triggered based on the scan
 * results obtained from an external scan (not aimed at roaming).
 * @DIAG_ROAM_REASON_BT_ACTIVITY: Roam triggered due to bluetooth
 * connection is established when the station is connected in 2.4 Ghz band.
 */
enum diag_roam_reason {
	DIAG_ROAM_REASON_UNKNOWN,
	DIAG_ROAM_REASON_PER,
	DIAG_ROAM_REASON_BEACON_MISS,
	DIAG_ROAM_REASON_POOR_RSSI,
	DIAG_ROAM_REASON_BETTER_RSSI,
	DIAG_ROAM_REASON_CONGESTION,
	DIAG_ROAM_REASON_USER_TRIGGER,
	DIAG_ROAM_REASON_BTM,
	DIAG_ROAM_REASON_BSS_LOAD,
	DIAG_ROAM_REASON_WTC,
	DIAG_ROAM_REASON_IDLE,
	DIAG_ROAM_REASON_DISCONNECTION,
	DIAG_ROAM_REASON_PERIODIC_TIMER,
	DIAG_ROAM_REASON_BACKGROUND_SCAN,
	DIAG_ROAM_REASON_BT_ACTIVITY
};

/**
 * enum diag_roam_sub_reason - Used by attribute
 * @DIAG_ROAM_SUB_REASON_PERIODIC_TIMER: Roam scan triggered due to periodic
 * timer expiry
 * @DIAG_ROAM_SUB_REASON_INACTIVITY_TIMER_LOW_RSSI: Roam scan trigger due
 * to no candidate found during LOW RSSI trigger.
 * @DIAG_ROAM_SUB_REASON_BTM_DI_TIMER: Roam scan triggered due to BTM Disassoc
 * Imminent timeout
 * @DIAG_ROAM_SUB_REASON_FULL_SCAN: Roam scan triggered due to partial scan
 * failure
 * @DIAG_ROAM_SUB_REASON_LOW_RSSI_PERIODIC: Roam trigger due to
 * emergency like deauth/disassoc.
 * @DIAG_ROAM_SUB_REASON_CU_PERIODIC: Roam trigger due to
 * BSS transition management request.
 * @DIAG_ROAM_SUB_REASON_PERIODIC_TIMER_AFTER_INACTIVITY_LOW_RSSI:
 * Roam scan triggered due to Low RSSI periodic timer
 * @DIAG_ROAM_SUB_REASON_PERIODIC_TIMER_AFTER_INACTIVITY_CU:
 * Roam trigger due to periodic timer after no candidate found during CU
 * inactivity timer scan.
 * @DIAG_ROAM_SUB_REASON_INACTIVITY_TIMER_CU: Roam trigger due to no candidate
 * found in high CU roam trigger.
 */

enum diag_roam_sub_reason {
	DIAG_ROAM_SUB_REASON_UNKNOWN = 0,
	DIAG_ROAM_SUB_REASON_PERIODIC_TIMER = 1,
	DIAG_ROAM_SUB_REASON_INACTIVITY_TIMER_LOW_RSSI = 2,
	DIAG_ROAM_SUB_REASON_BTM_DI_TIMER = 3,
	DIAG_ROAM_SUB_REASON_FULL_SCAN = 4,
	DIAG_ROAM_SUB_REASON_LOW_RSSI_PERIODIC = 5,
	DIAG_ROAM_SUB_REASON_CU_PERIODIC = 6,
	DIAG_ROAM_SUB_REASON_PERIODIC_TIMER_AFTER_INACTIVITY_LOW_RSSI = 7,
	DIAG_ROAM_SUB_REASON_PERIODIC_TIMER_AFTER_INACTIVITY_CU = 8,
	DIAG_ROAM_SUB_REASON_INACTIVITY_TIMER_CU = 9,
};
#endif

/*-------------------------------------------------------------------------
   Function declarations and documentation
   ------------------------------------------------------------------------*/
#ifdef FEATURE_WLAN_DIAG_SUPPORT
void host_diag_log_wlock(uint32_t reason, const char *wake_lock_name,
		uint32_t timeout, uint32_t status);
#else
static inline void host_diag_log_wlock(uint32_t reason,
		const char *wake_lock_name,
		uint32_t timeout, uint32_t status)
{

}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void host_log_low_resource_failure(uint8_t event_sub_type);
#else
static inline void host_log_low_resource_failure(uint8_t event_sub_type)
{

}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * host_log_rsn_info() - This function is used to send
 * requested rsn info in assoc request
 * @ucast_cipher: Unicast ciphers used in assoc request
 * @mcast_cipher: Group ciphers used in assoc request
 * @akm_suite: Gives information about akm suites used in assoc request
 * @group_mgmt: Requested group mgmt cipher suite
 *
 * This function is used to send RSN info used in assoc req to user space
 *
 * Return: None
 *
 */
void host_log_rsn_info(uint8_t *ucast_cipher, uint8_t *mcast_cipher,
		       uint8_t *auth_suite, uint8_t *gp_mgmt_cipher);

#else
static inline void host_log_rsn_info(uint8_t *ucast_cipher,
				     uint8_t *mcast_cipher,
				     uint8_t *auth_suite,
				     uint8_t *gp_mgmt_cipher)
{

}

#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * host_log_wlan_auth_info() - This function is used to send
 * algo num, seq num and status code for auth request
 * @auth_algo_num: Gives information about algo num used in auth request
 * @auth_tx_seq_num: seq num of auth request
 * @auth_status_code: status code of auth request
 *
 * This function is used to send send algo num, seq num and status code
 * for auth request
 *
 * Return: None
 *
 */
void
host_log_wlan_auth_info(uint16_t auth_algo_num, uint16_t auth_tx_seq_num,
			uint16_t auth_status_code);

#else
static inline void
host_log_wlan_auth_info(uint16_t auth_algo_num, uint16_t auth_tx_seq_num,
			uint16_t auth_status_code)
{
}

#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void qdf_wow_wakeup_host_event(uint8_t wow_wakeup_cause);

/**
 * host_log_acs_req_event() - ACS request event indication
 * @intf: network interface name for WLAN
 * @hw_mode: hw mode configured by hostapd
 * @bw: channel bandwidth (MHz)
 * @ht: a flag indicating whether HT phy mode is enabled
 * @vht: a flag indicating whether VHT phy mode is enabled
 * @chan_start: starting channel number for ACS scan
 * @chan_end: ending channel number for ACS scan
 *
 * Indicates the diag event for ACS request with payload related
 * to parameters populated by hostapd
 *
 * Return: None
 */
void host_log_acs_req_event(uint8_t *intf, const uint8_t *hw_mode,
			    uint16_t bw, uint8_t ht, uint8_t vht,
			    uint16_t chan_start, uint16_t chan_end);

/**
 * host_log_acs_scan_start() - ACS scan start event indication
 * @scan_id: scan request ID
 * @vdev_id: vdev/session ID
 *
 * Indicates the diag event for ACS scan start request
 *
 * Return: None
 */
void host_log_acs_scan_start(uint32_t scan_id, uint8_t vdev_id);

/**
 * host_log_acs_scan_done() - ACS scan done event indication
 * @status: indicating whether ACS scan is successful
 * @vdev_id: vdev/session ID
 * @scan_id: scan request ID
 *
 * Indicates the diag event for ACS scan done
 *
 * Return: None
 */
void host_log_acs_scan_done(const uint8_t *status, uint8_t vdev_id,
			    uint32_t scan_id);

/**
 * host_log_acs_chan_spect_weight() - ACS channel spectral weight indication
 * weight event indication
 * @chan: channel number
 * @weight: channel weight
 * @rssi: RSSI value obtained after scanning
 * @bss_count: number of BSS detected on this channel
 *
 * Indicates a diag event for ACS channel weight evaluation result
 *
 * Return: None
 */
void host_log_acs_chan_spect_weight(uint16_t chan, uint16_t weight,
				    int32_t rssi, uint16_t bss_count);

/**
 * host_log_acs_best_chan() - ACS best channel event indication
 * @chan: channel number
 * @weight: channel weight
 *
 * Indicates the best channel has been selected after ACS
 *
 * Return: None
 */
void host_log_acs_best_chan(uint16_t chan, uint16_t weight);

/**
 * host_log_device_status() - device status indication
 * @status_code: status code from enum wlan_bringup_status
 *
 * Indicates device status
 *
 * Return: None
 */
void host_log_device_status(uint16_t status_code);

#else
static inline void qdf_wow_wakeup_host_event(uint8_t wow_wakeup_cause)
{
	return;
}

static inline void host_log_acs_req_event(uint8_t *intf, const uint8_t *hw_mode,
					  uint16_t bw, uint8_t ht, uint8_t vht,
					  uint16_t chan_start,
					  uint16_t chan_end)
{
}

static inline void host_log_acs_scan_start(uint32_t scan_id, uint8_t vdev_id)
{
}

static inline void host_log_acs_scan_done(const uint8_t *status,
					  uint8_t vdev_id, uint32_t scan_id)
{
}

static inline void host_log_acs_chan_spect_weight(uint16_t chan,
						  uint16_t weight, int32_t rssi,
						  uint16_t bss_count)
{
}

static inline void host_log_acs_best_chan(uint16_t chan, uint32_t weight)
{
}

static inline void host_log_device_status(uint16_t status_code)
{
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __I_HOST_DIAG_CORE_EVENT_H */

/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
 * DOC: contains cp_stats structure definitions
 */

#ifndef _WLAN_CP_STATS_PUBLIC_STRUCTS_H_
#define _WLAN_CP_STATS_PUBLIC_STRUCTS_H_

#define CTRL_PATH_STATS_MAX_MAC_ADDR 1
#define CTRL_PATH_STATS_MAX_PDEV_ID 1
#define CTRL_PATH_STATS_MAX_VDEV_ID 1


#define INFRA_CP_STATS_MAX_REQ_TWT_DIALOG_ID 1

/*
 * Maximum of 1 TWT session can be supported per vdev.
 * This can be extended later to support more sessions.
 * if there is a request to retrieve stats for all existing
 * TWT sessions then response path can have multiple session
 * stats.
 */
#define INFRA_CP_STATS_MAX_RESP_TWT_DIALOG_ID 1

#ifdef WLAN_SUPPORT_TWT
/**
 * struct twt_infra_cp_stats_event - TWT statistics event structure
 * @vdev_id: virtual interface id
 * @peer_macaddr: peer mac address corresponding to a TWT session
 * @dialog_id: Represents dialog_id of the TWT session
 * @status:
 * @num_sp_cycles: Number of TWT service period elapsed so far
 * @avg_sp_dur_us: Average of actual wake duration observed so far
 * @min_sp_dur_us: Minimum value of wake duration observed across
 * @max_sp_dur_us: Maximum value of wake duration observed
 * @tx_mpdu_per_sp: Average number of MPDU's transmitted successfully
 * @rx_mpdu_per_sp: Average number of MPDU's received successfully
 * @tx_bytes_per_sp: Average number of bytes transmitted successfully
 * @rx_bytes_per_sp: Average number of bytes received successfully
 */
struct twt_infra_cp_stats_event {
	uint8_t vdev_id;
	struct qdf_mac_addr peer_macaddr;
	uint32_t dialog_id;
	uint32_t status;
	uint32_t num_sp_cycles;
	uint32_t avg_sp_dur_us;
	uint32_t min_sp_dur_us;
	uint32_t max_sp_dur_us;
	uint32_t tx_mpdu_per_sp;
	uint32_t rx_mpdu_per_sp;
	uint32_t tx_bytes_per_sp;
	uint32_t rx_bytes_per_sp;
};
#endif /* WLAN_SUPPORT_TWT */

#ifdef CONFIG_WLAN_BMISS
/**
 * struct bmiss_stats_rssi_samples - bmiss rssi samples structure
 * @rssi: dBm units
 * @sample_time: timestamp from host/target shared qtimer
 */
struct bmiss_stats_rssi_samples {
	int32_t rssi;
	uint32_t sample_time;
};

/**
 * struct consecutive_bmiss_stats - consecutive bmiss sats structure
 * @num_of_bmiss_sequences:number of consecutive bmiss > 2
 * @num_bitmask_wraparound:number of times bitmask wrapped around
 * @num_bcn_hist_lost:number of beacons history we have lost
 */
struct consecutive_bmiss_stats {
	uint32_t num_of_bmiss_sequences;
	uint32_t num_bitmask_wraparound;
	uint32_t num_bcn_hist_lost;
};

#define BMISS_STATS_RSSI_SAMPLES_MAX 10
/**
 * struct bmiss_infra_cp_stats_event -  bmiss statistics event structure
 * @vdev_id: virtual interface id
 * @peer_macaddr: peer mac address
 * @num_pre_bmiss: number of pre_bmiss
 * @rssi_samples: Rssi samples at pre bmiss
 * @rssi_sample_curr_index: current index of Rssi sampelse at pre bmiss
 * @num_first_bmiss: number of first bmiss
 * @num_final_bmiss: number of final bmiss
 * @num_null_sent_in_first_bmiss: number of null frames sent in first bmiss
 * @num_null_failed_in_first_bmiss: number of failed null frames in first bmiss
 * @num_null_sent_in_final_bmiss: number of null frames sent in final bmiss
 * @num_null_failed_in_final_bmiss: number of failed null frames in final bmiss
 * @cons_bmiss_stats: consecutive bmiss status
 */
struct bmiss_infra_cp_stats_event  {
	uint8_t vdev_id;
	struct qdf_mac_addr peer_macaddr;
	uint32_t num_pre_bmiss;
	struct bmiss_stats_rssi_samples rssi_samples[BMISS_STATS_RSSI_SAMPLES_MAX];
	uint32_t rssi_sample_curr_index;
	uint32_t num_first_bmiss;
	uint32_t num_final_bmiss;
	uint32_t num_null_sent_in_first_bmiss;
	uint32_t num_null_failed_in_first_bmiss;
	uint32_t num_null_sent_in_final_bmiss;
	uint32_t num_null_failed_in_final_bmiss;
	struct consecutive_bmiss_stats cons_bmiss_stats;
};
#endif /* CONFIG_WLAN_BMISS */

#ifdef WLAN_TELEMETRY_STATS_SUPPORT
/**
 * struct ctrl_path_pmlo_telemetry_stats_struct - pmlo telemetry
 * stats struct
 * @pdev_id: pdev_id for identifying the PHY
 * @dl_inbss_airtime_ac_be: ac_be airtime in dl inbss
 * @dl_inbss_airtime_ac_bk: ac_bk airtime in dl inbss
 * @dl_inbss_airtime_ac_vi: ac_vi airtime in dl inbss
 * @dl_inbss_airtime_ac_vo: ac_vo airtime in dl inbss
 * @ul_inbss_airtime_ac_be: ac_be airtime in ul inbss
 * @ul_inbss_airtime_ac_bk: ac_bk airtime in ul inbss
 * @ul_inbss_airtime_ac_vi: ac_vi airtime in ul inbss
 * @ul_inbss_airtime_ac_vo: ac_vo airtime in ul inbss
 * @estimated_air_time_ac_be: ac_be estimated air time
 * @estimated_air_time_ac_bk: ac_bk estimated air time
 * @estimated_air_time_ac_vi: ac_vi estimated air time
 * @estimated_air_time_ac_vo: ac_vo estimated air time
 * @avg_chan_lat_per_ac: array for Average channel latency per AC,
 * units in micro seconds.
 * @link_obss_airtime: Percentage of OBSS used air time per link,
 * units in percentage.
 * @link_idle_airtime: Idle/free airtime per link, units in percentage.
 * @ul_inbss_airtime_non_ac: ul inBSS airtime occupied by non-AC traffic,
 * units in percentage.
 * @dl_inbss_airtime_non_ac: dl inBSS airtime occupied by non-AC traffic,
 * units in percentage.
 */
struct ctrl_path_pmlo_telemetry_stats_struct {
	uint32_t pdev_id;
	uint32_t dl_inbss_airtime_ac_be : 8,
		 dl_inbss_airtime_ac_bk : 8,
		 dl_inbss_airtime_ac_vi : 8,
		 dl_inbss_airtime_ac_vo : 8;
	uint32_t ul_inbss_airtime_ac_be : 8,
		 ul_inbss_airtime_ac_bk : 8,
		 ul_inbss_airtime_ac_vi : 8,
		 ul_inbss_airtime_ac_vo : 8;
	uint32_t estimated_air_time_ac_be : 8,
		 estimated_air_time_ac_bk : 8,
		 estimated_air_time_ac_vi : 8,
		 estimated_air_time_ac_vo : 8;
	uint32_t avg_chan_lat_per_ac[WIFI_AC_MAX];
	uint32_t link_obss_airtime : 8,
		 link_idle_airtime : 8,
		 ul_inbss_airtime_non_ac : 8,
		 dl_inbss_airtime_non_ac : 8;
};
#endif

/**
 * struct infra_cp_stats_event - Event structure to store stats
 * @action: action for which this response was received
 *          (get/reset/start/stop)
 * @request_id: request cookie sent to Firmware in the command
 * @status: status of the infra_cp_stats command processing
 * @num_twt_infra_cp_stats: number of twt_infra_cp_stats buffers
 *                          available
 * @twt_infra_cp_stats: pointer to TWT session statistics structures
 * @bmiss_infra_cp_stats: pointer to beacon miss statistics
 * @ctrl_path_pmlo_telemetry_stats_struct: pointer to pmlo telemetry
 *                                         stats struct
 *
 * This structure is used to store the statistics information
 * extracted from firmware event(wmi_pdev_cp_fwstats_eventid)
 */
struct infra_cp_stats_event {
	uint32_t action;
	uint32_t request_id;
	uint32_t status;
#ifdef WLAN_SUPPORT_TWT
	uint32_t num_twt_infra_cp_stats;
	struct twt_infra_cp_stats_event *twt_infra_cp_stats;
#endif
#ifdef CONFIG_WLAN_BMISS
	struct bmiss_infra_cp_stats_event *bmiss_infra_cp_stats;
#endif
#ifdef WLAN_TELEMETRY_STATS_SUPPORT
	struct ctrl_path_pmlo_telemetry_stats_struct *telemetry_stats;
#endif
	/* Extend with other required infra_cp_stats structs */
};

enum infra_cp_stats_action {
	ACTION_REQ_CTRL_PATH_STAT_GET = 0,
	ACTION_REQ_CTRL_PATH_STAT_RESET,
	ACTION_REQ_CTRL_PATH_STAT_START,
	ACTION_REQ_CTRL_PATH_STAT_STOP,
	ACTION_REQ_CTRL_PATH_STAT_PERIODIC_PUBLISH,
};

enum infra_cp_stats_id {
	TYPE_REQ_CTRL_PATH_PDEV_TX_STAT = 0,
	TYPE_REQ_CTRL_PATH_VDEV_EXTD_STAT,
	TYPE_REQ_CTRL_PATH_MEM_STAT,
	TYPE_REQ_CTRL_PATH_TWT_STAT,
	TYPE_REQ_CTRL_PATH_BMISS_STAT,
	TYPE_REQ_CTRL_PATH_PMLO_STAT,
};

/**
 * struct infra_cp_stats_cmd_info - details of infra cp stats request
 * @stats_id: ID of the statistics type requested
 * @action: action to be performed (get/reset/start/stop)
 * @request_cookie: osif request cookie
 * @request_id: request id cookie to FW
 * @num_pdev_ids: number of pdev ids in the request
 * @pdev_id: array of pdev_ids
 * @num_vdev_ids: number of vdev ids in the request
 * @vdev_id: array of vdev_ids
 * @num_mac_addr_list: number of mac addresses in the request
 * @peer_mac_addr: array of mac addresses
 * @dialog_id: This is a TWT specific field. only one dialog_id
 *             can be specified for TWT stats. 0 to 254 are
 *             valid dialog_id's representing a single TWT session.
 *             255 represents all twt sessions
 * @infra_cp_stats_resp_cb: callback function to handle the response
 */
struct infra_cp_stats_cmd_info {
	enum infra_cp_stats_id stats_id;
	enum infra_cp_stats_action action;
	void *request_cookie;
	uint32_t request_id;
	uint32_t num_pdev_ids;
	uint32_t pdev_id[CTRL_PATH_STATS_MAX_PDEV_ID];
	uint32_t num_vdev_ids;
	uint32_t vdev_id[CTRL_PATH_STATS_MAX_VDEV_ID];
	uint32_t num_mac_addr_list;
	uint8_t peer_mac_addr[CTRL_PATH_STATS_MAX_MAC_ADDR][QDF_MAC_ADDR_SIZE];
#ifdef WLAN_SUPPORT_TWT
	uint32_t dialog_id;
#endif
	void (*infra_cp_stats_resp_cb)(struct infra_cp_stats_event *ev,
				       void *cookie);
	uint32_t stat_periodicity;
};
#endif

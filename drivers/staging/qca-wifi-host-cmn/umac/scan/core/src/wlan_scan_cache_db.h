/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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

/*
 * DOC: contains scan cache entry api
 */

#ifndef _WLAN_SCAN_CACHE_DB_H_
#define _WLAN_SCAN_CACHE_DB_H_

#include <scheduler_api.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_scan_public_structs.h>

#define SCAN_HASH_SIZE 64
#define SCAN_GET_HASH(addr) \
	(((const uint8_t *)(addr))[QDF_MAC_ADDR_SIZE - 1] % SCAN_HASH_SIZE)

#define ADJACENT_CHANNEL_RSSI_THRESHOLD -80

/**
 * struct scan_dbs - scan cache data base definition
 * @num_entries: number of scan entries
 * @scan_hash_tbl: link list of bssid hashed scan cache entries for a pdev
 */
struct scan_dbs {
	uint32_t num_entries;
	qdf_spinlock_t scan_db_lock;
	qdf_list_t scan_hash_tbl[SCAN_HASH_SIZE];
};

/**
 * struct scan_bcn_probe_event - beacon/probe info
 * @frm_type: frame type
 * @rx_data: mgmt rx data
 * @psoc: psoc pointer
 * @buf: rx frame
 */
struct scan_bcn_probe_event {
	uint32_t frm_type;
	struct mgmt_rx_event_params *rx_data;
	struct wlan_objmgr_psoc *psoc;
	qdf_nbuf_t buf;
};

/**
 * scm_handle_bcn_probe() - Process beacon and probe rsp
 * @msg: schedular msg with bcn info;
 *
 * API to handle the beacon/probe resp. msg->bodyptr will be consumed and freed
 * by this func
 *
 * Return: QDF status.
 */
QDF_STATUS scm_handle_bcn_probe(struct scheduler_msg *msg);

/**
 * __scm_handle_bcn_probe() - Process beacon and probe rsp
 * @bcn: beacon info;
 *
 * API to handle the beacon/probe resp. bcn will be consumed and freed by this
 * func
 *
 * Return: QDF status.
 */
QDF_STATUS __scm_handle_bcn_probe(struct scan_bcn_probe_event *bcn);

/**
 * scm_age_out_entries() - Age out entries older than aging time
 * @psoc: psoc pointer
 * @scan_db: scan database
 *
 * Return: void.
 */
void scm_age_out_entries(struct wlan_objmgr_psoc *psoc,
	struct scan_dbs *scan_db);

/**
 * scm_get_scan_result() - fetches scan result
 * @pdev: pdev info
 * @filter: Filters
 *
 * This function fetches scan result
 *
 * Return: scan list
 */
qdf_list_t *scm_get_scan_result(struct wlan_objmgr_pdev *pdev,
	struct scan_filter *filter);

/**
 * scm_purge_scan_results() - purge the scan list
 * @scan_result: scan list to be purged
 *
 * This function purge the temp scan list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_purge_scan_results(qdf_list_t *scan_result);

/**
 * scm_update_scan_mlme_info() - updates scan entry with mlme data
 * @pdev: pdev object
 * @scan_entry: source scan entry to read mlme info
 *
 * This function updates scan db with scan_entry->mlme_info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_update_scan_mlme_info(struct wlan_objmgr_pdev *pdev,
	struct scan_cache_entry *scan_entry);

/**
 * scm_flush_results() - flush scan entries matching the filter
 * @pdev: vdev object
 * @filter: filter to flush the scan entries
 *
 * Flush scan entries matching the filter.
 *
 * Return: QDF status.
 */
QDF_STATUS scm_flush_results(struct wlan_objmgr_pdev *pdev,
	struct scan_filter *filter);

/**
 * scm_filter_valid_channel() - The Public API to filter scan result
 * based on valid channel list
 * @pdev: pdev object
 * @chan_freq_list: valid channel frequency (in MHz) list
 * @num_chan: number of valid channels
 *
 * The Public API to to filter scan result
 * based on valid channel list.
 *
 * Return: void.
 */
void scm_filter_valid_channel(struct wlan_objmgr_pdev *pdev,
	uint32_t *chan_freq_list, uint32_t num_chan);

/**
 * scm_iterate_scan_db() - function to iterate scan table
 * @pdev: pdev object
 * @func: iterator function pointer
 * @arg: argument to be passed to func()
 *
 * API, this API iterates scan table and invokes func
 * on each scan enetry by passing scan entry and arg.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
scm_iterate_scan_db(struct wlan_objmgr_pdev *pdev,
	scan_iterator_func func, void *arg);

/**
 * scm_scan_register_bcn_cb() - API to register api to indicate bcn/probe
 * as soon as they are received
 * @pdev: psoc
 * @cb: callback to be registered
 * @type: Type of callback to be registered
 *
 * Return: enum scm_scan_status
 */
QDF_STATUS scm_scan_register_bcn_cb(struct wlan_objmgr_psoc *psoc,
	update_beacon_cb cb, enum scan_cb_type type);

/**
 * scm_scan_register_mbssid_cb() - API to register api to handle bcn/probe
 * as soon as they are generated
 * @psoc: psoc object
 * @cb: callback to be registered
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_scan_register_mbssid_cb(struct wlan_objmgr_psoc *psoc,
				       update_mbssid_bcn_prb_rsp cb);

/**
 * scm_db_init() - API to init scan db
 * @psoc: psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_db_init(struct wlan_objmgr_psoc *psoc);

/**
 * scm_db_deinit() - API to deinit scan db
 * @psoc: psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_db_deinit(struct wlan_objmgr_psoc *psoc);

#ifdef FEATURE_6G_SCAN_CHAN_SORT_ALGO

/**
 * scm_get_rnr_channel_db() - API to get rnr db
 * @psoc: psoc
 *
 * Return: rnr db
 */
struct channel_list_db *scm_get_rnr_channel_db(struct wlan_objmgr_psoc *psoc);

/**
 * scm_get_chan_meta() - API to return channel meta
 * @psoc: psoc
 * @freq: channel frequency
 *
 * Return: channel meta information
 */
struct meta_rnr_channel *scm_get_chan_meta(struct wlan_objmgr_psoc *psoc,
					   uint32_t chan_freq);

/**
 * scm_channel_list_db_init() - API to init scan list priority list db
 * @psoc: psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_channel_list_db_init(struct wlan_objmgr_psoc *psoc);

/**
 * scm_channel_list_db_deinit() - API to deinit scan list priority list db
 * @psoc: psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_channel_list_db_deinit(struct wlan_objmgr_psoc *psoc);

/**
 * scm_rnr_db_flush() - API to flush rnr entries
 * @psoc: psoc
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_rnr_db_flush(struct wlan_objmgr_psoc *psoc);

/**
 * scm_update_rnr_from_scan_cache() - API to update rnr info from scan cache
 * @pdev: pdev
 *
 * Return: void
 */
void scm_update_rnr_from_scan_cache(struct wlan_objmgr_pdev *pdev);

/**
 * scm_filter_rnr_flag_pno() - Remove FLAG_SCAN_ONLY_IF_RNR_FOUND flag
 *                             in channel if ssid is different for colocated AP,
 *                             during pno scan request
 * @vdev: vdev
 * @short_ssid: short ssid
 * @pno_chan_list: channel list
 *
 * Remove FLAG_SCAN_ONLY_IF_RNR_FOUND flag in channel if ssid is different for
 * colocated AP, in pno scan request
 *
 * Return: None
 */
void
scm_filter_rnr_flag_pno(struct wlan_objmgr_vdev *vdev,
			uint32_t short_ssid,
			struct chan_list *chan_list);

#else
static inline QDF_STATUS scm_channel_list_db_init(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS scm_channel_list_db_deinit(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
scm_filter_rnr_flag_pno(struct wlan_objmgr_vdev *vdev,
			uint32_t short_ssid,
			struct chan_list *chan_list)
{
}
#endif

/**
 * scm_scan_update_mlme_by_bssinfo() - updates scan entry with mlme data
 * @pdev: pdev object
 * @bss_info: BSS information
 *
 * This function updates scan db with scan_entry->mlme_info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS scm_scan_update_mlme_by_bssinfo(struct wlan_objmgr_pdev *pdev,
		struct bss_info *bss_info, struct mlme_info *mlme);

uint32_t scm_get_last_scan_time_per_channel(struct wlan_objmgr_vdev *vdev,
					    uint32_t freq);

/**
 * scm_scan_get_scan_entry_by_mac_freq() - Get scan entry by mac and freq
 * @pdev: pdev info
 * @bssid: BSSID of the bcn/probe response to be fetched from scan db
 * @freq: freq for scan filter
 * @cache_entry: cache entry to be filled from scan info
 *
 * Return: QDF_STATUS_SUCCESS if scan entry is present in scan db
 */
QDF_STATUS
scm_scan_get_scan_entry_by_mac_freq(struct wlan_objmgr_pdev *pdev,
				    struct qdf_mac_addr *bssid,
				    uint16_t freq,
				    struct scan_cache_entry
				    *cache_entry);

/**
 * scm_scan_get_entry_by_mac_addr() - Get bcn/probe rsp from scan db
 * @pdev: pdev info
 * @bssid: BSSID of the bcn/probe response to be fetched from scan db
 * @frame: Frame from scan db with given bssid.
 *
 * This API allocates the memory for bcn/probe rsp frame and returns
 * to caller through @frame->ptr. It's caller responsibility to free
 * the memory once it's done with the usage.
 *
 * Return: QDF_STATUS_SUCCESS if scan entry is present in db
 */
QDF_STATUS
scm_scan_get_entry_by_mac_addr(struct wlan_objmgr_pdev *pdev,
			       struct qdf_mac_addr *bssid,
			       struct element_info *frame);

/**
 * scm_scan_get_entry_by_bssid() - function to get scan entry by bssid
 * @pdev: pdev object
 * @bssid: bssid to be fetched from scan db
 *
 * Return : scan entry if found, else NULL
 */
struct scan_cache_entry *
scm_scan_get_entry_by_bssid(struct wlan_objmgr_pdev *pdev,
			    struct qdf_mac_addr *bssid);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * scm_get_mld_addr_by_link_addr() - function to fetch the peer mld address from
 * the scan entry for the given link address.
 * @pdev: pdev object
 * @link_addr: link address
 * @mld_mac_addr: pointer to mld_mac_address
 *
 * Return : scan entry if found, else NULL
 */
QDF_STATUS
scm_get_mld_addr_by_link_addr(struct wlan_objmgr_pdev *pdev,
			      struct qdf_mac_addr *link_addr,
			      struct qdf_mac_addr *mld_mac_addr);
#else
static inline QDF_STATUS
scm_get_mld_addr_by_link_addr(struct wlan_objmgr_pdev *pdev,
			      struct qdf_mac_addr *link_addr,
			      struct qdf_mac_addr *mld_mac_addr)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
#endif

/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Define PSOC MLME structure
 */
#ifndef _WLAN_PSOC_MLME_H_
#define _WLAN_PSOC_MLME_H_

#include <wlan_ext_mlme_obj_types.h>
#include <wlan_vdev_mgr_tgt_if_rx_defs.h>
#include <qdf_timer.h>
#include <wlan_cm_bss_score_param.h>
#ifdef FEATURE_VDEV_OPS_WAKELOCK
#include <target_if_psoc_wake_lock.h>
#endif

/* Max RNR size given max vaps are 16 */
#define MAX_RNR_SIZE (16 * 16 /* 11be compatible tbtt_info length */ \
			+ 4   /* Neighbor AP info field length */    \
			+ 2   /* RNR header bytes */)

/**
 * struct wlan_6ghz_rnr_global_cache - 6 GHz RNR cache buffer per soc
 * @rnr_buf: RNR cache buffer
 * @rnr_cnt: Count of APs in cache
 * @rnr_size: Size of RNR cache (RNR IE)
 */
struct wlan_6ghz_rnr_global_cache {
	char rnr_buf[MAX_RNR_SIZE];
	int rnr_cnt;
	uint16_t rnr_size;
};

/**
 * struct psoc_phy_config - psoc phy caps
 * @vdev_nss_24g: Number of NSS the device support in 2.4Ghz
 * @vdev_nss_5g: Number of NSS the device support in 5Ghz
 * @ht_cap: If dev is configured as HT capable
 * @vht_cap:If dev is configured as VHT capable
 * @he_cap: If dev is configured as HE capable
 * @eht_cap: If dev is configured as EHT capable
 * @vht_24G_cap:If dev is configured as VHT capable for 2.4Ghz
 * @beamformee_cap:If dev is configured as BF capable
 * @bw_above_20_5ghz: BW greater than 20Mhz supported for 5Ghz
 * @bw_above_20_24ghz: BW greater than 20Mhz supported for 2.4Ghz
 * @max_chan_switch_ie: If max channel switch IE is supported
 */
struct psoc_phy_config {
	uint8_t vdev_nss_24g;
	uint8_t vdev_nss_5g;
	uint32_t ht_cap:1,
		 vht_cap:1,
		 he_cap:1,
		 eht_cap:1,
		 vht_24G_cap:1,
		 beamformee_cap:1,
		 bw_above_20_5ghz:1,
		 bw_above_20_24ghz:1,
		 max_chan_switch_ie:1;
};

/**
 * struct psoc_mlo_config - psoc mlo config
 * @reconfig_reassoc_en: If reassoc on ML reconfig AP addition is enabled
 */
struct psoc_mlo_config {
	uint8_t reconfig_reassoc_en;
};

/**
 * struct psoc_config - psoc level configs
 * @score_config:          BSS scoring related config
 * @phy_config:            Psoc Phy config
 * @mlo_config:            Psoc mlo config
 */
struct psoc_config {
	struct scoring_cfg score_config;
	struct psoc_phy_config phy_config;
	struct psoc_mlo_config mlo_config;
};

#ifdef WLAN_FEATURE_PEER_TRANS_HIST
/**
 * enum wlan_peer_tbl_trans_entry_flags - Peer table transition flags
 * @WLAN_PEER_TBL_TRANS_CREATE: Peer table modify due to create
 * @WLAN_PEER_TBL_TRANS_DESTROY: Peer table modify due to destroy
 * @WLAN_PEER_TBL_TRANS_CONNECT: Peer table modify during connect
 * @WLAN_PEER_TBL_TRANS_DISCONNECT: Peer table modify during disconnect
 * @WLAN_PEER_TBL_TRANS_ROAM: Peer table modify during roaming
 * @WLAN_PEER_TBL_TRANS_LINK_SWITCH: Peer table modify during link switch
 */
enum wlan_peer_tbl_trans_entry_flags {
	WLAN_PEER_TBL_TRANS_CREATE = BIT(0),
	WLAN_PEER_TBL_TRANS_DESTROY = BIT(1),
	WLAN_PEER_TBL_TRANS_CONNECT = BIT(2),
	WLAN_PEER_TBL_TRANS_DISCONNECT = BIT(3),
	WLAN_PEER_TBL_TRANS_ROAM = BIT(4),
	WLAN_PEER_TBL_TRANS_LINK_SWITCH = BIT(5),
};

/**
 * struct wlan_peer_tbl_trans_entry - Peer transition history node
 * @node: Node of the entry
 * @ts: Timestamp of peer table modify
 * @vdev_id: Peer's vdev's ID
 * @opmode: Peer's VDEV opmode
 * @is_mlo: %true if peer's VDEV MLO else %false
 * @is_mlo_link: %true if peer's VDEV mlo link, else %false
 * @is_primary: %true if this peer is primary
 * @is_first_link: %true if this peer is the first link, else %false
 * @auth_status: Status of auth from FW roam sync
 * @num_roam_links: Count of roamed link from FW roam sync
 * @peer_addr: MAC address of peer
 * @peer_mld_addr: MLD address of peer
 * @peer_flags: Peer flags from enum wlan_peer_tbl_trans_entry_flags
 */
struct wlan_peer_tbl_trans_entry {
	qdf_list_node_t node;
	qdf_time_t ts;
	uint8_t vdev_id;
	enum QDF_OPMODE opmode;
	uint8_t is_mlo:1,
		is_mlo_link:1,
		is_primary:1,
		is_first_link:1,
		auth_status:2,
		num_roam_links:2;
	struct qdf_mac_addr peer_addr;
	struct qdf_mac_addr peer_mld_addr;
	unsigned long peer_flags;
};
#endif

/**
 * struct psoc_mlme_obj -  PSoC MLME component object
 * @psoc:                  PSoC object
 * @ext_psoc_ptr:          PSoC legacy pointer
 * @psoc_vdev_rt:          PSoC Vdev response timer
 * @psoc_mlme_wakelock:    Wakelock to prevent system going to suspend
 * @rnr_6ghz_cache:        Cache of 6Ghz vap in RNR ie format
 * @rnr_6ghz_cache_legacy: Legacy (13TBTT) cache of 6Ghz vap in RNR ie format
 * @psoc_cfg:              Psoc level configs
 * @peer_history_list:     Peer table transition history
 */
struct psoc_mlme_obj {
	struct wlan_objmgr_psoc *psoc;
	mlme_psoc_ext_t *ext_psoc_ptr;
	struct vdev_response_timer psoc_vdev_rt[WLAN_UMAC_PSOC_MAX_VDEVS];
#ifdef FEATURE_VDEV_OPS_WAKELOCK
	struct psoc_mlme_wakelock psoc_mlme_wakelock;
#endif
	struct wlan_6ghz_rnr_global_cache rnr_6ghz_cache[WLAN_UMAC_MAX_PDEVS];
	struct wlan_6ghz_rnr_global_cache rnr_6ghz_cache_legacy[WLAN_UMAC_MAX_PDEVS];
	struct psoc_config psoc_cfg;
#ifdef WLAN_FEATURE_PEER_TRANS_HIST
	qdf_list_t peer_history_list;
#endif
};

#endif

/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef WMA_TGT_CFG_H
#define WMA_TGT_CFG_H

#include "wma_sar_public_structs.h"
#include "nan_public_structs.h"

/**
 * struct wma_tgt_services - target services
 * @sta_power_save: sta power save
 * @uapsd: uapsd
 * @ap_dfs: ap dfs
 * @en_11ac: enable 11ac
 * @arp_offload: arp offload
 * @early_rx: early rx
 * @pno_offload: pno offload
 * @beacon_offload: beacon offload
 * @lte_coex_ant_share: LTE coex ant share
 * @en_tdls: enable tdls
 * @en_tdls_offchan: enable tdls offchan
 * @en_tdls_uapsd_buf_sta: enable sta tdls uapsd buf
 * @en_tdls_uapsd_sleep_sta: enable sta tdls uapsd sleep
 * @en_tdls_wideband_support: Get TDLS wideband support
 * @en_tdls_11ax_support: Get TDLS ax support
 * @en_tdls_6g_support: Get TDLS 6g fw capability
 * @en_tdls_mlo_support: Get TDLS mlo fw support
 * @en_n_link_mlo_support: Get N-Link mlo fw support
 * @en_roam_offload: enable roam offload
 * @en_11ax: enable 11ax
 * @is_fw_mawc_capable: Motion Aided Wireless Connectivity feature
 * @twt_requestor: TWT requestor capability
 * @twt_responder: TWT responder capability
 * @bcn_reception_stats: Beacon Reception stats capability
 * @is_roam_scan_ch_to_host: Get roam scan channels from fw supported
 * @ll_stats_per_chan_rx_tx_time: Per channel tx and rx time support in ll stats
 * @is_get_station_clubbed_in_ll_stats_req: Get station req support within ll
 *                                          stats req
 * @is_fw_therm_throt_supp: Get thermal throttling threshold
 * @igmp_offload_enable: Get igmp offload enable or disable
 * @en_11be: enable 11be
 * @dynamic_vdev_macaddr_support: Dynamic update of vdev MAC addr is
 *                                supported or not
 * @is_mlo_per_link_stats_supported: Per link mlo stats is supported or not
 * @en_mlo_tid_to_link_support: Get tid to link fw support
 */
struct wma_tgt_services {
	uint32_t sta_power_save;
	bool uapsd;
	uint32_t ap_dfs;
	uint32_t en_11ac;
	uint32_t arp_offload;
	uint32_t early_rx;
#ifdef FEATURE_WLAN_SCAN_PNO
	bool pno_offload;
#endif /* FEATURE_WLAN_SCAN_PNO */
	bool beacon_offload;
	bool pmf_offload;
	uint32_t lte_coex_ant_share;
#ifdef FEATURE_WLAN_TDLS
	bool en_tdls;
	bool en_tdls_offchan;
	bool en_tdls_uapsd_buf_sta;
	bool en_tdls_uapsd_sleep_sta;
	bool en_tdls_wideband_support;
#ifdef WLAN_FEATURE_11AX
	bool en_tdls_11ax_support;
	bool en_tdls_6g_support;
#endif
#ifdef WLAN_FEATURE_11BE
	bool en_tdls_mlo_support;
	bool en_n_link_mlo_support;
#endif
#endif /* FEATURE_WLAN_TDLS */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	bool en_roam_offload;
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
	bool en_11ax;
	bool get_peer_info_enabled;
	bool is_fils_roaming_supported;
	bool is_fw_mawc_capable;
	bool is_11k_offload_supported;
	bool twt_requestor;
	bool twt_responder;
	bool obss_scan_offload;
	bool bcn_reception_stats;
	bool is_roam_scan_ch_to_host;
	bool ll_stats_per_chan_rx_tx_time;
#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
	bool is_get_station_clubbed_in_ll_stats_req;
#endif
	bool is_fw_therm_throt_supp;
#ifdef WLAN_FEATURE_IGMP_OFFLOAD
	bool igmp_offload_enable;
#endif
	bool en_11be;
#ifdef WLAN_FEATURE_DYNAMIC_MAC_ADDR_UPDATE
	bool dynamic_vdev_macaddr_support;
#endif
#ifdef WLAN_FEATURE_11BE_MLO
	bool is_mlo_per_link_stats_supported;
#endif
#ifdef WLAN_FEATURE_11BE
	bool en_mlo_tid_to_link_support;
#endif
};

/**
 * struct wma_tgt_ht_cap - ht capabalitiy
 * @mpdu_density: mpdu density
 * @ht_rx_stbc: ht rx stbc
 * @ht_tx_stbc: ht tx stbc
 * @ht_rx_ldpc: ht rx ldpc
 * @ht_sgi_20: ht sgi 20
 * @ht_sgi_40: ht sgi 40
 * @num_rf_chains: num of rf chains
 * @dynamic_smps: Dynamic MIMO powersave
 */
struct wma_tgt_ht_cap {
	uint32_t mpdu_density;
	bool ht_rx_stbc;
	bool ht_tx_stbc;
	bool ht_rx_ldpc;
	bool ht_sgi_20;
	bool ht_sgi_40;
	uint32_t num_rf_chains;
	bool dynamic_smps;
};

/**
 * struct wma_tgt_vht_cap - vht capabalities
 * @vht_max_mpdu: vht max mpdu
 * @supp_chan_width: supported channel width
 * @vht_rx_ldpc: vht rx ldpc
 * @vht_short_gi_80: vht short gi 80
 * @vht_short_gi_160: vht short gi 160
 * @vht_tx_stbc: vht tx stbc
 * @vht_rx_stbc: vht rx stbc
 * @vht_su_bformer: vht su bformer
 * @vht_su_bformee: vht su bformee
 * @vht_mu_bformer: vht mu bformer
 * @vht_mu_bformee: vht mu bformee
 * @vht_max_ampdu_len_exp: vht max ampdu len exp
 * @vht_txop_ps: vht txop ps
 * @vht_mcs_10_11_supp: VHT MCS 10 & 11 support
 */
struct wma_tgt_vht_cap {
	uint32_t vht_max_mpdu;
	uint32_t supp_chan_width;
	uint32_t vht_rx_ldpc;
	uint32_t vht_short_gi_80;
	uint32_t vht_short_gi_160;
	uint32_t vht_tx_stbc;
	uint32_t vht_rx_stbc;
	uint32_t vht_su_bformer;
	uint32_t vht_su_bformee;
	uint32_t vht_mu_bformer;
	uint32_t vht_mu_bformee;
	uint32_t vht_max_ampdu_len_exp;
	uint32_t vht_txop_ps;
	uint32_t vht_mcs_10_11_supp;
};

/**
 * struct wma_tgt_aux_dev_caps - aux capability in wma layer
 * @supported_modes_bitmap: each bit define in WMI_AUX_DEV_CAPS_SUPPORTED_MODE
 * @listen_pdev_id_map: define which AUX MAC can listen/scan for the HW mode
 * @emlsr_pdev_id_map: define which AUX MAC can perform eMLSR for the HW mode
 */
struct wma_tgt_aux_dev_caps {
	uint32_t supported_modes_bitmap;
	uint32_t listen_pdev_id_map;
	uint32_t emlsr_pdev_id_map;
};

/**
 * struct board_info - Structure for board related information
 * @bdf_version: board file version
 * @ref_design_id: reference design id
 * @customer_id: customer id
 * @project_id: project id
 * @board_data_rev: board data revision
 *
 * This board information will be stored in board file during the
 * calibration and customization.
 *
 */
struct board_info {
	uint32_t bdf_version;
	uint32_t ref_design_id;
	uint32_t customer_id;
	uint32_t project_id;
	uint32_t board_data_rev;
};

/**
 * struct wma_tgt_cfg - target config
 * @target_fw_version: target fw version
 * @target_fw_vers_ext: target fw extended sub version
 * @band_cap: band capability bitmap
 * @reg_domain: reg domain
 * @eeprom_rd_ext: eeprom rd ext
 * @hw_macaddr: hw mcast addr
 * @services: struct wma_tgt_services
 * @ht_cap: struct wma_tgt_ht_cap
 * @vht_cap: struct wma_tgt_vht_cap
 * @max_intf_count: max interface count
 * @lpss_support: lpass support
 * @egap_support: enhanced green ap support
 * @nan_datapath_enabled: nan data path support
 * @he_cap: HE capability received from FW
 * @dfs_cac_offload: dfs and cac timer offloaded
 * @tx_bfee_8ss_enabled: Tx Beamformee support for 8x8
 * @dynamic_nss_chains_update: per vdev dynamic nss, chains update
 * @rcpi_enabled: for checking rcpi support
 * @obss_detection_offloaded: obss detection offloaded to firmware
 * @obss_color_collision_offloaded: obss color collision offloaded to firmware
 * @sar_version: Version of SAR supported by firmware
 * @legacy_bcast_twt_support: broadcast twt support
 * @restricted_80p80_bw_supp: Restricted 80+80MHz(165MHz BW) support
 * @twt_bcast_req_support: twt bcast requestor support
 * @twt_bcast_res_support: twt bcast responder support
 * @twt_nudge_enabled: twt nudge enable
 * @all_twt_enabled: all twt enabled
 * @twt_stats_enabled: twt stats enabled
 *
 */
struct wma_tgt_cfg {
	uint32_t target_fw_version;
	uint32_t target_fw_vers_ext;
	uint32_t band_cap;
	uint32_t reg_domain;
	uint32_t eeprom_rd_ext;
	struct qdf_mac_addr hw_macaddr;
	struct wma_tgt_services services;
	struct wma_tgt_ht_cap ht_cap;
	struct wma_tgt_vht_cap vht_cap;
	uint8_t max_intf_count;
#ifdef WLAN_FEATURE_LPSS
	uint8_t lpss_support;
#endif
	uint8_t ap_arpns_support;
	uint32_t fine_time_measurement_cap;
#ifdef WLAN_FEATURE_NAN
	bool nan_datapath_enabled;
#endif
	bool sub_20_support;
	uint16_t wmi_max_len;
#ifdef WLAN_FEATURE_11AX
	tDot11fIEhe_cap he_cap;
	uint8_t ppet_2g[HE_MAX_PPET_SIZE];
	uint8_t ppet_5g[HE_MAX_PPET_SIZE];
	tDot11fIEhe_cap he_cap_2g;
	tDot11fIEhe_cap he_cap_5g;
	uint16_t he_mcs_12_13_supp_2g;
	uint16_t he_mcs_12_13_supp_5g;
#endif
	bool dfs_cac_offload;
	bool tx_bfee_8ss_enabled;
	bool dynamic_nss_chains_support;
	bool rcpi_enabled;
	bool obss_detection_offloaded;
	bool obss_color_collision_offloaded;
	uint32_t hw_bd_id;
	struct board_info hw_bd_info;
	enum sar_version sar_version;
	struct nan_tgt_caps nan_caps;
	bool legacy_bcast_twt_support;
	bool restricted_80p80_bw_supp;
#ifdef WLAN_SUPPORT_TWT
	bool twt_bcast_req_support;
	bool twt_bcast_res_support;
	bool twt_nudge_enabled;
	bool all_twt_enabled;
	bool twt_stats_enabled;
#endif
#ifdef WLAN_FEATURE_11BE
	tDot11fIEeht_cap eht_cap;
	tDot11fIEeht_cap eht_cap_2g;
	tDot11fIEeht_cap eht_cap_5g;
#endif
	struct wma_tgt_aux_dev_caps wma_aux0_dev_caps[WMI_HOST_HW_MODE_MAX];
};
#endif /* WMA_TGT_CFG_H */

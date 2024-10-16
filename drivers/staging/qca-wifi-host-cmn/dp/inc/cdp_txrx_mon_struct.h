/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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

 /**
 * @file cdp_txrx_mon_struct.h
 * @brief Define the monitor mode API structure
 * shared by data path and the OS interface module
 */

#ifndef _CDP_TXRX_MON_STRUCT_H_
#define _CDP_TXRX_MON_STRUCT_H_

#ifdef QCA_SUPPORT_LITE_MONITOR

#define CDP_LITE_MON_PEER_MAX 16

#define CDP_MON_FRM_TYPE_MAX 3
#define CDP_MON_FRM_FILTER_MODE_MAX 4

#define CDP_LITE_MON_LEN_64B 0x40
#define CDP_LITE_MON_LEN_128B 0x80
#define CDP_LITE_MON_LEN_256B 0x100
#define CDP_LITE_MON_LEN_FULL 0xFFFF

#define CDP_LITE_MON_FILTER_ALL 0xFFFF

/* This should align with nac mac type enumerations in ieee80211_ioctl.h */
#define CDP_LITE_MON_PEER_MAC_TYPE_CLIENT 2

/**
 * enum cdp_lite_mon legacy_filter: legacy filters for tx/rx
 * @LEGACY_FILTER_DISABLED: No filter / filter disabled
 * @LEGACY_FILTER_MCOPY: M_Copy filter
 * @LEGACY_FILTER_TX_CAPTURE: Tx_Capture filter
 * @LEGACY_FILTER_RX_ENH_CAPTURE: Rx Enhance capture filter
 * @LEGACY_FILTER_ADV_MON_FILTER: Advance Monitor filter
 *
 * Use to identify which filter is currently enabled using lite mon
 */
enum cdp_lite_mon_legacy_filter {
	LEGACY_FILTER_DISABLED = 0,
	LEGACY_FILTER_MCOPY = 1,
	LEGACY_FILTER_TX_CAPTURE = 2,
	LEGACY_FILTER_RX_ENH_CAPTURE = 3,
	LEGACY_FILTER_ADV_MON_FILTER = 4,
};

/* lite mon frame levels */
enum cdp_lite_mon_level {
	/* level invalid */
	CDP_LITE_MON_LEVEL_INVALID = 0,
	/* level msdu */
	CDP_LITE_MON_LEVEL_MSDU = 1,
	/* level mpdu */
	CDP_LITE_MON_LEVEL_MPDU = 2,
	/* level ppdu */
	CDP_LITE_MON_LEVEL_PPDU = 3,
};

/* lite mon peer action */
enum cdp_lite_mon_peer_action {
	/* peer add */
	CDP_LITE_MON_PEER_ADD = 0,
	/* peer remove */
	CDP_LITE_MON_PEER_REMOVE = 1,
};

/* lite mon config direction */
enum cdp_lite_mon_direction {
	/* lite mon config direction rx */
	CDP_LITE_MON_DIRECTION_RX = 1,
	/* lite mon config direction tx */
	CDP_LITE_MON_DIRECTION_TX = 2,
};
#endif
/* MU max user to sniff */
#define CDP_MU_SNIF_USER_MAX 4
/* EHT max type and compression mode */
#define CDP_EHT_TYPE_MODE_MAX 3
/* Same as MAX_20MHZ_SEGMENTS */
#define CDP_MAX_20MHZ_SEGS 16
/* Same as MAX_ANTENNA_EIGHT */
#define CDP_MAX_NUM_ANTENNA 8

/* XXX not really a mode; there are really multiple PHY's */
enum cdp_mon_phymode {
	/* autoselect */
	CDP_IEEE80211_MODE_AUTO	= 0,
	/* 5GHz, OFDM */
	CDP_IEEE80211_MODE_11A	= 1,
	/* 2GHz, CCK */
	CDP_IEEE80211_MODE_11B	= 2,
	/* 2GHz, OFDM */
	CDP_IEEE80211_MODE_11G	= 3,
	/* 2GHz, GFSK */
	CDP_IEEE80211_MODE_FH	= 4,
	/* 5GHz, OFDM, 2x clock dynamic turbo */
	CDP_IEEE80211_MODE_TURBO_A	= 5,
	   /* 2GHz, OFDM, 2x clock dynamic turbo */
	CDP_IEEE80211_MODE_TURBO_G	= 6,
	/* 5Ghz, HT20 */
	CDP_IEEE80211_MODE_11NA_HT20	= 7,
	/* 2Ghz, HT20 */
	CDP_IEEE80211_MODE_11NG_HT20	= 8,
	/* 5Ghz, HT40 (ext ch +1) */
	CDP_IEEE80211_MODE_11NA_HT40PLUS	= 9,
	/* 5Ghz, HT40 (ext ch -1) */
	CDP_IEEE80211_MODE_11NA_HT40MINUS = 10,
	  /* 2Ghz, HT40 (ext ch +1) */
	CDP_IEEE80211_MODE_11NG_HT40PLUS = 11,
	/* 2Ghz, HT40 (ext ch -1) */
	CDP_IEEE80211_MODE_11NG_HT40MINUS = 12,
	/* 2Ghz, Auto HT40 */
	CDP_IEEE80211_MODE_11NG_HT40	= 13,
	/* 5Ghz, Auto HT40 */
	CDP_IEEE80211_MODE_11NA_HT40	= 14,
	/* 5Ghz, VHT20 */
	CDP_IEEE80211_MODE_11AC_VHT20	= 15,
	/* 5Ghz, VHT40 (Ext ch +1) */
	CDP_IEEE80211_MODE_11AC_VHT40PLUS   = 16,
	/* 5Ghz  VHT40 (Ext ch -1) */
	CDP_IEEE80211_MODE_11AC_VHT40MINUS  = 17,
	/* 5Ghz, VHT40 */
	CDP_IEEE80211_MODE_11AC_VHT40	= 18,
	/* 5Ghz, VHT80 */
	CDP_IEEE80211_MODE_11AC_VHT80	= 19,
	/* 5Ghz, VHT160 */
	CDP_IEEE80211_MODE_11AC_VHT160	= 20,
	/* 5Ghz, VHT80_80 */
	CDP_IEEE80211_MODE_11AC_VHT80_80	= 21,
};

enum {
	CDP_PKT_TYPE_OFDM = 0,
	CDP_PKT_TYPE_CCK,
	CDP_PKT_TYPE_HT,
	CDP_PKT_TYPE_VHT,
	CDP_PKT_TYPE_HE,
	CDP_PKT_TYPE_EHT,
	CDP_PKT_TYPE_NO_SUP,
	CDP_PKT_TYPE_MAX,
};

enum {
	CDP_SGI_0_8_US = 0,
	CDP_SGI_0_4_US,
	CDP_SGI_1_6_US,
	CDP_SGI_3_2_US,
};

enum {
	CDP_RX_TYPE_SU = 0,
	CDP_RX_TYPE_MU_MIMO,
	CDP_RX_TYPE_MU_OFDMA,
	CDP_RX_TYPE_MU_OFDMA_MIMO,
	CDP_RX_TYPE_MAX,
};

enum {
	CDP_MU_TYPE_DL = 0,
	CDP_MU_TYPE_UL,
	CDP_MU_TYPE_MAX,
};

/*
 *Band Width Types
 */
enum CMN_BW_TYPES {
	CMN_BW_20MHZ,
	CMN_BW_40MHZ,
	CMN_BW_80MHZ,
	CMN_BW_160MHZ,
	CMN_BW_80_80MHZ,
#ifdef WLAN_FEATURE_11BE
	CMN_BW_320MHZ,
#endif
	CMN_BW_CNT,
	CMN_BW_IDLE = 0xFF, /*default BW state */
};

enum cdp_punctured_modes {
	NO_PUNCTURE,
#ifdef WLAN_FEATURE_11BE
	PUNCTURED_20MHZ,
	PUNCTURED_40MHZ,
	PUNCTURED_80MHZ,
	PUNCTURED_120MHZ,
#endif
	PUNCTURED_MODE_CNT,
};

struct cdp_mon_status {
	/* bss color value 1-63 used for update on ppdu_desc bsscolor */
	uint8_t bsscolor;
	int rs_numchains;
	int rs_flags;
#define IEEE80211_RX_FCS_ERROR      0x01
#define IEEE80211_RX_MIC_ERROR      0x02
#define IEEE80211_RX_DECRYPT_ERROR  0x04
/* holes in flags here between, ATH_RX_XXXX to IEEE80211_RX_XXX */
#define IEEE80211_RX_KEYMISS        0x200
#define IEEE80211_RX_PN_ERROR       0x400
	int rs_rssi;       /* RSSI (noise floor adjusted) */
	int rs_abs_rssi;   /* absolute RSSI */
	int rs_datarate;   /* data rate received */
	int rs_rateieee;
	int rs_ratephy1;
	int rs_ratephy2;
	int rs_ratephy3;

/* Keep the same as ATH_MAX_ANTENNA */
#define IEEE80211_MAX_ANTENNA       3
	/* RSSI (noise floor adjusted) */
	u_int8_t    rs_rssictl[IEEE80211_MAX_ANTENNA];
	/* RSSI (noise floor adjusted) */
	u_int8_t    rs_rssiextn[IEEE80211_MAX_ANTENNA];
	/* rs_rssi is valid or not */
	u_int8_t    rs_isvalidrssi;

	enum cdp_mon_phymode rs_phymode;
	int         rs_freq;

	union {
		u_int8_t            data[8];
		u_int64_t           tsf;
	} rs_tstamp;

	/*
	 * Detail channel structure of recv frame.
	 * It could be NULL if not available
	 */


#ifdef ATH_SUPPORT_AOW
	u_int16_t   rs_rxseq;      /* WLAN Sequence number */
#endif
#ifdef ATH_VOW_EXT_STATS
	/* Lower 16 bits holds the udp checksum offset in the data pkt */
	u_int32_t vow_extstats_offset;
	/* Higher 16 bits contains offset in the data pkt at which vow
	 * ext stats are embedded
	 */
#endif
	u_int8_t rs_isaggr;
	u_int8_t rs_isapsd;
	int16_t rs_noisefloor;
	u_int16_t  rs_channel;
#ifdef ATH_SUPPORT_TxBF
	u_int32_t   rs_rpttstamp;   /* txbf report time stamp*/
#endif

	/* The following counts are meant to assist in stats calculation.
	 * These variables are incremented only in specific situations, and
	 * should not be relied upon for any purpose other than the original
	 * stats related purpose they have been introduced for.
	*/

	u_int16_t   rs_cryptodecapcount; /* Crypto bytes decapped/demic'ed. */
	u_int8_t    rs_padspace;         /* No. of padding bytes present after
					  header in wbuf. */
	u_int8_t    rs_qosdecapcount;    /* QoS/HTC bytes decapped. */

	/* End of stats calculation related counts. */

	/*
	 * uint8_t     rs_lsig[IEEE80211_LSIG_LEN];
	 * uint8_t     rs_htsig[IEEE80211_HTSIG_LEN];
	 * uint8_t     rs_servicebytes[IEEE80211_SB_LEN];
	 * uint8_t     rs_fcs_error;
	*/

	/* cdp convergence monitor mode status */
	union {
		u_int8_t            cdp_data[8];
		u_int64_t           cdp_tsf;
	} cdp_rs_tstamp;

	uint8_t  cdp_rs_pream_type;
	uint32_t cdp_rs_user_rssi;
	uint8_t  cdp_rs_stbc;
	uint8_t  cdp_rs_sgi;
	uint32_t cdf_rs_rate_mcs;
	uint32_t cdp_rs_reception_type;
	uint32_t cdp_rs_bw;
	uint32_t cdp_rs_nss;
	uint8_t  cdp_rs_fcs_err;
	bool     cdp_rs_rxdma_err;
};

enum {
	CDP_MON_PPDU_START = 0,
	CDP_MON_PPDU_END,
};

#ifdef QCA_UNDECODED_METADATA_SUPPORT
/**
 * enum cdp_mon_phyrx_abort_reason_code: Phy err code to store the reason
 * why PHY generated an abort request.
 */
enum cdp_mon_phyrx_abort_reason_code {
	CDP_PHYRX_ERR_PHY_OFF = 0,
	CDP_PHYRX_ERR_SYNTH_OFF,
	CDP_PHYRX_ERR_OFDMA_TIMING,
	CDP_PHYRX_ERR_OFDMA_SIGNAL_PARITY,
	CDP_PHYRX_ERR_OFDMA_RATE_ILLEGAL,
	CDP_PHYRX_ERR_OFDMA_LENGTH_ILLEGAL,
	CDP_PHYRX_ERR_OFDMA_RESTART,
	CDP_PHYRX_ERR_OFDMA_SERVICE,
	CDP_PHYRX_ERR_PPDU_OFDMA_POWER_DROP,
	CDP_PHYRX_ERR_CCK_BLOKKER,
	CDP_PHYRX_ERR_CCK_TIMING = 10,
	CDP_PHYRX_ERR_CCK_HEADER_CRC,
	CDP_PHYRX_ERR_CCK_RATE_ILLEGAL,
	CDP_PHYRX_ERR_CCK_LENGTH_ILLEGAL,
	CDP_PHYRX_ERR_CCK_RESTART,
	CDP_PHYRX_ERR_CCK_SERVICE,
	CDP_PHYRX_ERR_CCK_POWER_DROP,
	CDP_PHYRX_ERR_HT_CRC_ERR,
	CDP_PHYRX_ERR_HT_LENGTH_ILLEGAL,
	CDP_PHYRX_ERR_HT_RATE_ILLEGAL,
	CDP_PHYRX_ERR_HT_ZLF = 20,
	CDP_PHYRX_ERR_FALSE_RADAR_EXT,
	CDP_PHYRX_ERR_GREEN_FIELD,
	CDP_PHYRX_ERR_BW_GT_DYN_BW,
	CDP_PHYRX_ERR_HT_LSIG_RATE_MISMATCH,
	CDP_PHYRX_ERR_VHT_CRC_ERROR,
	CDP_PHYRX_ERR_VHT_SIGA_UNSUPPORTED,
	CDP_PHYRX_ERR_VHT_LSIG_LEN_INVALID,
	CDP_PHYRX_ERR_VHT_NDP_OR_ZLF,
	CDP_PHYRX_ERR_VHT_NSYM_LT_ZERO,
	CDP_PHYRX_ERR_VHT_RX_EXTRA_SYMBOL_MISMATCH = 30,
	CDP_PHYRX_ERR_VHT_RX_SKIP_GROUP_ID0,
	CDP_PHYRX_ERR_VHT_RX_SKIP_GROUP_ID1TO62,
	CDP_PHYRX_ERR_VHT_RX_SKIP_GROUP_ID63,
	CDP_PHYRX_ERR_OFDM_LDPC_DECODER_DISABLED,
	CDP_PHYRX_ERR_DEFER_NAP,
	CDP_PHYRX_ERR_FDOMAIN_TIMEOUT,
	CDP_PHYRX_ERR_LSIG_REL_CHECK,
	CDP_PHYRX_ERR_BT_COLLISION,
	CDP_PHYRX_ERR_UNSUPPORTED_MU_FEEDBACK,
	CDP_PHYRX_ERR_PPDU_TX_INTERRUPT_RX = 40,
	CDP_PHYRX_ERR_UNSUPPORTED_CBF,
	CDP_PHYRX_ERR_OTHER,
	CDP_PHYRX_ERR_HE_SIGA_UNSUPPORTED,
	CDP_PHYRX_ERR_HE_SIGA_CRC_ERROR,
	CDP_PHYRX_ERR_HE_SIGB_UNSUPPORTED,
	CDP_PHYRX_ERR_HE_SIGB_CRC_ERROR,
	CDP_PHYRX_ERR_HE_MU_MODE_UNSUPPORTED,
	CDP_PHYRX_ERR_HE_NDP_OR_ZLF,
	CDP_PHYRX_ERR_HE_NSYM_LT_ZERO,
	CDP_PHYRX_ERR_HE_RU_PARAMS_UNSUPPORTED = 50,
	CDP_PHYRX_ERR_HE_NUM_USERS_UNSUPPORTED,
	CDP_PHYRX_ERR_HE_SOUNDING_PARAMS_UNSUPPORTED,
	CDP_PHYRX_ERR_HE_EXT_SU_UNSUPPORTED,
	CDP_PHYRX_ERR_HE_TRIG_UNSUPPORTED,
	CDP_PHYRX_ERR_HE_LSIG_LEN_INVALID = 55,
	CDP_PHYRX_ERR_HE_LSIG_RATE_MISMATCH,
	CDP_PHYRX_ERR_OFDMA_SIGNAL_RELIABILITY,
	CDP_PHYRX_ERR_HT_NSYM_LT_ZERO,
	CDP_PHYRX_ERR_VHT_LSIG_RATE_MISMATCH,
	CDP_PHYRX_ERR_VHT_PAID_GID_MISMATCH = 60,
	CDP_PHYRX_ERR_VHT_UNSUPPORTED_BW,
	CDP_PHYRX_ERR_VHT_GI_DISAM_MISMATCH,
	CDP_PHYRX_ERR_RX_WDG_TIMEOUT = 63,
	CDP_PHYRX_ERR_MAX
};
#endif

#define MAX_PPDU_ID_HIST 128

/**
 * struct cdp_pdev_mon_stats
 * @status_ppdu_state: state on PPDU start and end
 * @status_ppdu_start: status ring PPDU start TLV count
 * @status_ppdu_end: status ring PPDU end TLV count
 * @status_ppdu_compl: status ring matching start and end count on PPDU
 * @status_ppdu_start_mis: status ring missing start TLV count on PPDU
 * @status_ppdu_end_mis: status ring missing end TLV count on PPDU
 * @mpdu_cnt_fcs_ok: MPDU ok count per pkt and reception type DL-UL and user
 * @mpdu_cnt_fcs_err: MPDU err count per pkt and reception type DL-UL and user
 * @ppdu_eht_type_mode: PPDU count per type compression mode and DL-UL
 * @end_user_stats_cnt: PPDU end user TLV count
 * @start_user_info_cnt: PPDU start user info TLV count
 * @status_ppdu_done: status ring PPDU done TLV count
 * @dest_ppdu_done: destination ring PPDU count
 * @dest_mpdu_done: destination ring MPDU count
 * @dup_mon_linkdesc_cnt: duplicate link descriptor indications from HW
 * @dup_mon_buf_cnt: duplicate buffer indications from HW
 * @tlv_tag_status_err: status not correct in the tlv tag
 * @status_buf_done_war: Number of status ring buffers for which DMA not done
 *  WAR is applied.
 * @mon_rx_bufs_replenished_dest: Rx buffers replenish count
 * @mon_rx_bufs_reaped_dest: Rx buffer reap count
 * @ppdu_id_mismatch: counter to track ppdu id mismatch in
 *  mointor status and monitor destination ring
 * @ppdu_id_match: counter to track ppdu id match in
 *  mointor status and monitor destination ring
 * @status_ppdu_drop: Number of ppdu dropped from monitor status ring
 * @dest_ppdu_drop: Number of ppdu dropped from monitor destination ring
 * @mon_link_desc_invalid: msdu link desc invalid count
 * @mon_rx_desc_invalid: rx_desc invalid count
 * @mpdu_ppdu_id_mismatch_drop: mpdu's ppdu id did not match destination
 *  ring ppdu id
 * @mpdu_decap_type_invalid: mpdu decap type invalid count
 * @rx_undecoded_count: Received undecoded frame count
 * @rx_undecoded_error: Rx undecoded errors
 * @rx_hdr_not_received: Rx HDR not received for MPDU
 * @parent_buf_alloc: Numder of parent nbuf allocated for MPDU
 * @parent_buf_free: Number of parent nbuf freed
 * @pkt_buf_count: Number of packet buffers received
 * @mpdus_to_stack: Number of MPDUs delivered to stack
 * @status_buf_count: Number of status buffer received
 * @empty_desc_ppdu: Number of empty desc received
 * @total_ppdu_info_enq: Number of PPDUs enqueued to wq
 * @total_ppdu_info_drop: Number of PPDUs dropped
 * @total_ppdu_info_alloc: Number of PPDU info allocated
 * @total_ppdu_info_free: Number of PPDU info freed
 * @ppdu_drop_cnt: Total PPDU drop count
 * @mpdu_drop_cnt: Total MPDU drop count
 * @end_of_ppdu_drop_cnt: Total end of ppdu drop count
 * @tlv_drop_cnt: TLV drop count
 */
struct cdp_pdev_mon_stats {
#ifndef REMOVE_MON_DBG_STATS
	uint32_t status_ppdu_state;
	uint32_t status_ppdu_start;
	uint32_t status_ppdu_end;
	uint32_t status_ppdu_compl;
	uint32_t status_ppdu_start_mis;
	uint32_t status_ppdu_end_mis;
#endif
	uint32_t mpdu_cnt_fcs_ok[CDP_PKT_TYPE_MAX][CDP_RX_TYPE_MAX]
				[CDP_MU_TYPE_MAX][CDP_MU_SNIF_USER_MAX];
	uint32_t mpdu_cnt_fcs_err[CDP_PKT_TYPE_MAX][CDP_RX_TYPE_MAX]
				 [CDP_MU_TYPE_MAX][CDP_MU_SNIF_USER_MAX];
	uint32_t ppdu_eht_type_mode[CDP_EHT_TYPE_MODE_MAX][CDP_MU_TYPE_MAX];
	uint32_t end_user_stats_cnt;
	uint32_t start_user_info_cnt;
	uint32_t status_ppdu_done;
	uint32_t dest_ppdu_done;
	uint32_t dest_mpdu_done;
	uint32_t dest_mpdu_drop;
	uint32_t dup_mon_linkdesc_cnt;
	uint32_t dup_mon_buf_cnt;
	uint32_t stat_ring_ppdu_id_hist[MAX_PPDU_ID_HIST];
	uint32_t dest_ring_ppdu_id_hist[MAX_PPDU_ID_HIST];
	uint32_t ppdu_id_hist_idx;
	uint32_t mon_rx_dest_stuck;
	uint32_t tlv_tag_status_err;
	uint32_t status_buf_done_war;
	uint32_t mon_rx_bufs_replenished_dest;
	uint32_t mon_rx_bufs_reaped_dest;
	uint32_t ppdu_id_mismatch;
	uint32_t ppdu_id_match;
	uint32_t status_ppdu_drop;
	uint32_t dest_ppdu_drop;
	uint32_t mon_link_desc_invalid;
	uint32_t mon_rx_desc_invalid;
	uint32_t mon_nbuf_sanity_err;
	uint32_t mpdu_ppdu_id_mismatch_drop;
	uint32_t mpdu_decap_type_invalid;
#ifdef QCA_UNDECODED_METADATA_SUPPORT
	uint32_t rx_undecoded_count;
	uint32_t rx_undecoded_error[CDP_PHYRX_ERR_MAX];
#endif
	uint32_t rx_hdr_not_received;
	uint32_t parent_buf_alloc;
	uint32_t parent_buf_free;
	uint32_t pkt_buf_count;
	uint32_t mpdus_buf_to_stack;
	uint32_t status_buf_count;
	uint32_t empty_desc_ppdu;
	uint32_t total_ppdu_info_enq;
	uint32_t total_ppdu_info_drop;
	uint32_t total_ppdu_info_alloc;
	uint32_t total_ppdu_info_free;
	uint32_t ppdu_drop_cnt;
	uint32_t mpdu_drop_cnt;
	uint32_t end_of_ppdu_drop_cnt;
	uint32_t tlv_drop_cnt;
};

#ifdef QCA_SUPPORT_LITE_MONITOR
/**
 * cdp_lite_mon_filter_config - lite mon set/get filter config
 * @direction: direction tx/rx
 * @disable: disables lite mon
 * @level: MSDU/MPDU/PPDU levels
 * @metadata: meta information to be added
 * @mgmt_filter: mgmt filter for modes fp,md,mo
 * @ctrl_filter: ctrl filter for modes fp,md,mo
 * @data_filter: data filter for modes fp,md,mo
 * @len: mgmt/ctrl/data frame lens
 * @debug: debug options
 * @vdev_id: output vdev id
 * @legacy_filter_enabled: legacy filter currently enabled
 */
struct cdp_lite_mon_filter_config {
	uint8_t direction;
	uint8_t disable;
	uint8_t level;
	uint8_t metadata;
	uint16_t mgmt_filter[CDP_MON_FRM_FILTER_MODE_MAX];
	uint16_t ctrl_filter[CDP_MON_FRM_FILTER_MODE_MAX];
	uint16_t data_filter[CDP_MON_FRM_FILTER_MODE_MAX];
	uint16_t len[CDP_MON_FRM_TYPE_MAX];
	uint8_t debug;
	uint8_t vdev_id;
	uint8_t legacy_filter_enabled;
};

/**
 * cdp_lite_mon_peer_config - lite mon set peer config
 * @direction: direction tx/rx
 * @action: add/del
 * @vdev_id: peer vdev id
 * @mac: peer mac
 */
struct cdp_lite_mon_peer_config {
	uint8_t direction;
	uint8_t action;
	uint8_t vdev_id;
	uint8_t mac[QDF_MAC_ADDR_SIZE];
};

/**
 * cdp_lite_mon_peer_info - lite mon get peer config
 * @direction: direction tx/rx
 * @count: no of peers
 * @mac: peer macs
 */
struct cdp_lite_mon_peer_info {
	uint8_t direction;
	uint8_t count;
	uint8_t mac[CDP_LITE_MON_PEER_MAX][QDF_MAC_ADDR_SIZE];
};
#endif
/* channel operating width */
enum cdp_channel_width {
	CHAN_WIDTH_20 = 0,
	CHAN_WIDTH_40,
	CHAN_WIDTH_80,
	CHAN_WIDTH_160,
	CHAN_WIDTH_80P80,
	CHAN_WIDTH_5,
	CHAN_WIDTH_10,
	CHAN_WIDTH_165,
	CHAN_WIDTH_160P160,
	CHAN_WIDTH_320,

	CHAN_WIDTH_MAX,
};

/* struct cdp_rssi_temp_off_param_dp
 * @rssi_temp_offset: Temperature based rssi offset , send every 30 secs
 */

struct cdp_rssi_temp_off_param_dp {
	int32_t rssi_temp_offset;
};

/*
 * struct cdp_rssi_dbm_conv_param_dp
 * @curr_bw: Current bandwidth
 * @curr_rx_chainmask: Current rx chainmask
 * @xbar_config: 4 bytes, used for BB to RF Chain mapping
 * @xlna_bypass_offset: Low noise amplifier bypass offset
 * @xlna_bypass_threshold: Low noise amplifier bypass threshold
 * @nfHwDbm: HW noise floor in dBm per chain, per 20MHz subband
 */
struct cdp_rssi_dbm_conv_param_dp {
	uint32_t curr_bw;
	uint32_t curr_rx_chainmask;
	uint32_t xbar_config;
	int32_t xlna_bypass_offset;
	int32_t xlna_bypass_threshold;
	int8_t nf_hw_dbm[CDP_MAX_NUM_ANTENNA][CDP_MAX_20MHZ_SEGS];
};

/*
 * struct cdp_rssi_db2dbm_param_dp
 * @pdev_id: pdev_id
 * @rssi_temp_off_present: to check temp offset values present or not
 * @rssi_dbm_info_present: to check rssi dbm conversion parameters
 *						   present or not
 * @temp_off_param: cdp_rssi_temp_off_param_dp structure value
 * @rssi_dbm_param: cdp_rssi_dbm_conv_param_dp staructure value
 */
struct cdp_rssi_db2dbm_param_dp {
	uint32_t pdev_id;
	bool rssi_temp_off_present;
	bool rssi_dbm_info_present;
	struct cdp_rssi_temp_off_param_dp temp_off_param;
	struct cdp_rssi_dbm_conv_param_dp rssi_dbm_param;
};

/*
 * enum cdp_mon_reap_source: trigger source of the reap timer of
 * monitor status ring
 * @CDP_MON_REAP_SOURCE_PKTLOG: pktlog
 * @CDP_MON_REAP_SOURCE_CFR: CFR
 * @CDP_MON_REAP_SOURCE_EMESH: easy mesh
 * @CDP_MON_REAP_SOURCE_NUM: total number of the sources
 * @CDP_MON_REAP_SOURCE_ANY: any of the sources
 */
enum cdp_mon_reap_source {
	CDP_MON_REAP_SOURCE_PKTLOG,
	CDP_MON_REAP_SOURCE_CFR,
	CDP_MON_REAP_SOURCE_EMESH,

	/* keep last */
	CDP_MON_REAP_SOURCE_NUM,
	CDP_MON_REAP_SOURCE_ANY,
};
#endif

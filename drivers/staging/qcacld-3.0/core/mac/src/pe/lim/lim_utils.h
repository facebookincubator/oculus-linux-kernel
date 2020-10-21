/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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
 * This file lim_utils.h contains the utility definitions
 * LIM uses.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_UTILS_H
#define __LIM_UTILS_H

#include "sir_api.h"
#include "sir_debug.h"

#include "lim_types.h"
#include "lim_scan_result_utils.h"
#include "lim_timer_utils.h"
#include "lim_trace.h"
#include "include/wlan_vdev_mlme.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "wlan_qct_sys.h"

typedef enum {
	ONE_BYTE = 1,
	TWO_BYTE = 2
} eSizeOfLenField;

#define LIM_AID_MASK                              0xC000
#define LIM_SPECTRUM_MANAGEMENT_BIT_MASK          0x0100
#define LIM_RRM_BIT_MASK                          0x1000
#define LIM_SHORT_PREAMBLE_BIT_MASK               0x0020
#define LIM_IMMEDIATE_BLOCK_ACK_MASK              0x8000
#define LIM_MAX_REASSOC_RETRY_LIMIT            2

#define VHT_MCS_3x3_MASK    0x30
#define VHT_MCS_2x2_MASK    0x0C

#define CENTER_FREQ_DIFF_160MHz 8
#define CENTER_FREQ_DIFF_80P80MHz 16

#define CH_TO_CNTR_FREQ_DIFF_160MHz 14
#define CH_TO_CNTR_FREQ_DIFF_80MHz 6

#define IS_VHT_NSS_1x1(__mcs_map)	((__mcs_map & 0xFFFC) == 0xFFFC)

#define MGMT_RX_PACKETS_THRESHOLD 200

/* 11B AP detection bit position */
#define OBSS_DETECTION_11B_AP_BIT_MASK       0x0001
/* 11B STA detection bit position */
#define OBSS_DETECTION_11B_STA_BIT_MASK      0x0002
/* 11G AP detection bit position */
#define OBSS_DETECTION_11G_AP_BIT_MASK       0x0004
/* 11A AP detection bit position */
#define OBSS_DETECTION_11A_BIT_MASK          0x0008
/* HT legacy detection bit position */
#define OBSS_DETECTION_HT_LEGACY_BIT_MASK    0x0010
/* HT mixed detection bit position */
#define OBSS_DETECTION_HT_MIXED_BIT_MASK     0x0020
/* HT 20mhz detection bit position */
#define OBSS_DETECTION_HT_20MHZ_BIT_MASK     0x0040

/**
 * OBSS detection period in ms, used by firmware to decide
 * absent detection and also gap between same detection ind.
 */
#define OBSS_DETECTION_PERIOD_MS             4000

/* To check if 11B AP detection bit set */
#define OBSS_DETECTION_IS_11B_AP(_m) ((_m) & OBSS_DETECTION_11B_AP_BIT_MASK)
/* To check if 11B STA detection bit set */
#define OBSS_DETECTION_IS_11B_STA(_m) ((_m) & OBSS_DETECTION_11B_STA_BIT_MASK)
/* To check if 11G AP detection bit set */
#define OBSS_DETECTION_IS_11G_AP(_m) ((_m) & OBSS_DETECTION_11G_AP_BIT_MASK)
/* To check if 11A AP detection bit set */
#define OBSS_DETECTION_IS_11A(_m) ((_m) & OBSS_DETECTION_11A_BIT_MASK)
/* To check if HT legacy detection bit set */
#define OBSS_DETECTION_IS_HT_LEGACY(_m) \
	((_m) & OBSS_DETECTION_HT_LEGACY_BIT_MASK)
/* To check if HT mixed detection bit set */
#define OBSS_DETECTION_IS_HT_MIXED(_m) ((_m) & OBSS_DETECTION_HT_MIXED_BIT_MASK)
/* To check if HT 20mhz detection bit set */
#define OBSS_DETECTION_IS_HT_20MHZ(_m) ((_m) & OBSS_DETECTION_HT_20MHZ_BIT_MASK)

#define MAX_WAIT_FOR_BCN_TX_COMPLETE 4000
#define MAX_WAKELOCK_FOR_CSA         5000

#ifdef WLAN_FEATURE_11W
typedef union uPmfSaQueryTimerId {
	struct {
		uint8_t sessionId;
		uint16_t peerIdx;
	} fields;
	uint32_t value;
} tPmfSaQueryTimerId, *tpPmfSaQueryTimerId;
#endif

typedef struct last_processed_frame {
	tSirMacAddr sa;
	uint16_t seq_num;
} last_processed_msg;

/**
 * struct lim_max_tx_pwr_attr - List of tx powers from various sources
 * @reg_max: power from regulatory database
 * @ap_tx_power: local power constraint adjusted value
 * @ini_tx_power: Max tx power from ini config
 * @frequency: current operating frequency for which above powers are defined
 */
struct lim_max_tx_pwr_attr {
	int8_t reg_max;
	int8_t ap_tx_power;
	uint8_t ini_tx_power;
	uint32_t frequency;
};

/* LIM utility functions */
bool lim_is_valid_frame(last_processed_msg *last_processed_frm,
		uint8_t *pRxPacketInfo);
void lim_update_last_processed_frame(last_processed_msg *last_processed_frm,
		uint8_t *pRxPacketInfo);

char *lim_dot11_reason_str(uint16_t reasonCode);
char *lim_mlm_state_str(tLimMlmStates state);
char *lim_sme_state_str(tLimSmeStates state);
char *lim_msg_str(uint32_t msgType);
char *lim_result_code_str(tSirResultCodes resultCode);
char *lim_dot11_mode_str(struct mac_context *mac, uint8_t dot11Mode);
void lim_print_mlm_state(struct mac_context *mac, uint16_t logLevel,
		tLimMlmStates state);
void lim_print_sme_state(struct mac_context *mac, uint16_t logLevel,
		tLimSmeStates state);
void lim_print_msg_name(struct mac_context *mac, uint16_t logLevel, uint32_t msgType);

QDF_STATUS lim_send_set_max_tx_power_req(struct mac_context *mac,
		int8_t txPower,
		struct pe_session *pe_session);

/**
 * lim_get_max_tx_power() - Utility to get maximum tx power
 * @mac: mac handle
 * @attr: pointer to buffer containing list of tx powers
 *
 * This function is used to get the maximum possible tx power from the list
 * of tx powers mentioned in @attr.
 *
 * Return: Max tx power
 */
uint8_t lim_get_max_tx_power(struct mac_context *mac,
			     struct lim_max_tx_pwr_attr *attr);

/* AID pool management functions */
void lim_init_peer_idxpool(struct mac_context *, struct pe_session *);
uint16_t lim_assign_peer_idx(struct mac_context *, struct pe_session *);

void lim_enable_overlap11g_protection(struct mac_context *mac,
		tpUpdateBeaconParams pBeaconParams,
		tpSirMacMgmtHdr pMh,
		struct pe_session *pe_session);
void lim_update_overlap_sta_param(struct mac_context *mac, tSirMacAddr bssId,
		tpLimProtStaParams pStaParams);
void lim_update_short_preamble(struct mac_context *mac, tSirMacAddr peerMacAddr,
		tpUpdateBeaconParams pBeaconParams,
		struct pe_session *pe_session);
void lim_update_short_slot_time(struct mac_context *mac, tSirMacAddr peerMacAddr,
		tpUpdateBeaconParams pBeaconParams,
		struct pe_session *pe_session);

/*
 * lim_send_sme_mgmt_frame_ind() - Function to send mgmt frame ind to HDD
 * @mac_ctx: Pointer to Global MAC structure
 * @frame_type: Type of mgmt frame
 * @frame: Frame pointer
 * @frame_len: Length og mgmt frame
 * @session_id: session id
 * @psession_entry: PE Session Entry
 * @rx_channel: Channel of where packet is received
 * @rx_rssi: rssi value
 * @rx_flags: RXMGMT flags to be set for the frame. Defined in enum rxmgmt_flags
 *
 * Indicate the Mgmt Frame received to SME to HDD callback
 * handle Probe_req/Action frame currently
 *
 * Return: None
*/
void lim_send_sme_mgmt_frame_ind(struct mac_context *mac_ctx, uint8_t frame_type,
				 uint8_t *frame, uint32_t frame_len,
				 uint16_t session_id, uint32_t rx_channel,
				 struct pe_session *psession_entry,
				 int8_t rx_rssi, enum rxmgmt_flags rx_flags);

/*
 * lim_deactivate_timers() - Function to deactivate lim timers
 * @mac_ctx: Pointer to global mac structure
 *
 *	This function is used to deactivate lim timers
 *
 * Return: None
 */
void lim_deactivate_timers(struct mac_context *mac_ctx);

/*
 * The below 'product' check tobe removed if 'Association' is
 * allowed in IBSS.
 */
void lim_release_peer_idx(struct mac_context *, uint16_t, struct pe_session *);

void lim_decide_ap_protection(struct mac_context *mac, tSirMacAddr peerMacAddr,
		tpUpdateBeaconParams pBeaconParams, struct pe_session *);
void lim_decide_ap_protection_on_delete(struct mac_context *mac,
		tpDphHashNode sta,
		tpUpdateBeaconParams pBeaconParams,
		struct pe_session *pe_session);

QDF_STATUS lim_update_11a_protection(struct mac_context *mac,
		uint8_t enable,
		uint8_t overlap,
		tpUpdateBeaconParams pBeaconParams,
		struct pe_session *);
QDF_STATUS lim_enable11g_protection(struct mac_context *mac,
		uint8_t enable,
		uint8_t overlap,
		tpUpdateBeaconParams pBeaconParams,
		struct pe_session *pe_session);
QDF_STATUS lim_enable_ht_protection_from11g(struct mac_context *mac,
		uint8_t enable,
		uint8_t overlap,
		tpUpdateBeaconParams
		pBeaconParams,
		struct pe_session *pe_session);
QDF_STATUS lim_enable_ht20_protection(struct mac_context *mac,
		uint8_t enable, uint8_t overlap,
		tpUpdateBeaconParams pBeaconParams,
		struct pe_session *pe_session);
QDF_STATUS lim_enable_ht_non_gf_protection(struct mac_context *mac,
		uint8_t enable, uint8_t overlap,
		tpUpdateBeaconParams
		pBeaconParams, struct pe_session *);
QDF_STATUS lim_enable_ht_rifs_protection(struct mac_context *mac,
		uint8_t enable, uint8_t overlap,
		tpUpdateBeaconParams
		pBeaconParams,
		struct pe_session *pe_session);
QDF_STATUS lim_enable_ht_lsig_txop_protection(struct mac_context *mac,
		uint8_t enable,
		uint8_t overlap,
		tpUpdateBeaconParams
		pBeaconParams, struct pe_session *);
QDF_STATUS lim_enable_short_preamble(struct mac_context *mac,
		uint8_t enable,
		tpUpdateBeaconParams pBeaconParams,
		struct pe_session *pe_session);
QDF_STATUS lim_enable_ht_obss_protection(struct mac_context *mac,
		uint8_t enable, uint8_t overlap,
		tpUpdateBeaconParams
		pBeaconParams, struct pe_session *);
void lim_decide_sta_protection(struct mac_context *mac,
		tpSchBeaconStruct pBeaconStruct,
		tpUpdateBeaconParams pBeaconParams,
		struct pe_session *pe_session);
void lim_decide_sta_protection_on_assoc(struct mac_context *mac,
		tpSchBeaconStruct pBeaconStruct,
		struct pe_session *pe_session);
void lim_update_sta_run_time_ht_switch_chnl_params(struct mac_context *mac,
		tDot11fIEHTInfo *pHTInfo,
		uint8_t bss_idx,
		struct pe_session *pe_session);
/* Print MAC address utility function */
void lim_print_mac_addr(struct mac_context *, tSirMacAddr, uint8_t);

/* Deferred Message Queue read/write */
uint8_t lim_write_deferred_msg_q(struct mac_context *mac,
				 struct scheduler_msg *limMsg);
struct scheduler_msg *lim_read_deferred_msg_q(struct mac_context *mac);
void lim_handle_defer_msg_error(struct mac_context *mac,
				struct scheduler_msg *pLimMsg);

/* Deferred Message Queue Reset */
void lim_reset_deferred_msg_q(struct mac_context *mac);

static inline void lim_sys_process_mmh_msg_api(struct mac_context *mac,
					       struct scheduler_msg *msg)
{
	sys_process_mmh_msg(mac, msg);
}

void lim_handle_update_olbc_cache(struct mac_context *mac);

uint8_t lim_is_null_ssid(tSirMacSSid *pSsid);

/* 11h Support */
void lim_stop_tx_and_switch_channel(struct mac_context *mac, uint8_t sessionId);

/**
 * lim_process_channel_switch_timeout() - Process chanel switch timeout
 * @mac: pointer to Global MAC structure
 *
 * Return: none
 */
void lim_process_channel_switch_timeout(struct mac_context *);
QDF_STATUS lim_start_channel_switch(struct mac_context *mac,
		struct pe_session *pe_session);
void lim_update_channel_switch(struct mac_context *, tpSirProbeRespBeacon,
		struct pe_session *pe_session);

void lim_switch_primary_channel(struct mac_context *, uint8_t, struct pe_session *);
void lim_switch_primary_secondary_channel(struct mac_context *mac,
					struct pe_session *pe_session,
					uint8_t newChannel,
					uint8_t ch_center_freq_seg0,
					uint8_t ch_center_freq_seg1,
					enum phy_ch_width ch_width);
void lim_update_sta_run_time_ht_capability(struct mac_context *mac,
		tDot11fIEHTCaps *pHTCaps);
void lim_update_sta_run_time_ht_info(struct mac_context *mac,
		tDot11fIEHTInfo *pRcvdHTInfo,
		struct pe_session *pe_session);
void lim_cancel_dot11h_channel_switch(struct mac_context *mac,
		struct pe_session *pe_session);
bool lim_is_channel_valid_for_channel_switch(struct mac_context *mac,
		uint8_t channel);
QDF_STATUS lim_restore_pre_channel_switch_state(struct mac_context *mac,
		struct pe_session *pe_session);

void lim_prepare_for11h_channel_switch(struct mac_context *mac,
		struct pe_session *pe_session);
void lim_switch_channel_cback(struct mac_context *mac, QDF_STATUS status,
		uint32_t *data, struct pe_session *pe_session);

/**
 * lim_assoc_rej_get_remaining_delta() - Get remaining time delta for
 * the rssi based disallowed list entry
 * @node: rssi based disallowed list entry
 *
 * Return: remaining delta, can be -ve if time has already expired.
 */
int
lim_assoc_rej_get_remaining_delta(struct sir_rssi_disallow_lst *node);

/**
 * lim_rem_blacklist_entry_with_lowest_delta() - Remove the entry with lowest
 * time delta
 * @list: rssi based rejected BSSID list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
lim_rem_blacklist_entry_with_lowest_delta(qdf_list_t *list);

/**
 * lim_get_session_by_macaddr() - api to find session based on MAC
 * @mac_ctx: Pointer to global mac structure.
 * @self_mac: MAC address.
 *
 * This function is used to get session for given MAC address.
 *
 * Return: session pointer if exists, NULL otherwise.
 */
struct csr_roam_session *lim_get_session_by_macaddr(struct mac_context *mac_ctx,
		tSirMacAddr self_mac);

static inline enum band_info lim_get_rf_band(uint8_t channel)
{
	if ((channel >= SIR_11A_CHANNEL_BEGIN) &&
			(channel <= SIR_11A_CHANNEL_END))
		return BAND_5G;

	if ((channel >= SIR_11B_CHANNEL_BEGIN) &&
			(channel <= SIR_11B_CHANNEL_END))
		return BAND_2G;

	return BAND_UNKNOWN;
}

static inline QDF_STATUS
lim_get_mgmt_staid(struct mac_context *mac, uint16_t *staid,
		struct pe_session *pe_session)
{
	if (LIM_IS_AP_ROLE(pe_session))
		*staid = 1;
	else if (LIM_IS_STA_ROLE(pe_session))
		*staid = 0;
	else
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

static inline int lim_select_cb_mode(tDphHashNode *sta,
		struct pe_session *pe_session, uint8_t channel,
		uint8_t chan_bw)
{
	if (sta->mlmStaContext.vhtCapability && chan_bw) {
		if (channel == 36 || channel == 52 || channel == 100 ||
				channel == 116 || channel == 149) {
			return PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW - 1;
		} else if (channel == 40 || channel == 56 || channel == 104 ||
				channel == 120 || channel == 153) {
			return PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW - 1;
		} else if (channel == 44 || channel == 60 || channel == 108 ||
				channel == 124 || channel == 157) {
			return PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH - 1;
		} else if (channel == 48 || channel == 64 || channel == 112 ||
				channel == 128 || channel == 161) {
			return PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH - 1;
		} else if (channel == 165) {
			return PHY_SINGLE_CHANNEL_CENTERED;
		}
	} else if (sta->mlmStaContext.htCapability) {
		if (channel == 40 || channel == 48 || channel == 56 ||
			channel == 64 || channel == 104 || channel == 112 ||
			channel == 120 || channel == 128 || channel == 136 ||
			channel == 144 || channel == 153 || channel == 161) {
			return PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		} else if (channel == 36 || channel == 44 || channel == 52 ||
				channel == 60 || channel == 100 ||
				channel == 108 || channel == 116 ||
				channel == 124 || channel == 132 ||
				channel == 140 || channel == 149 ||
				channel == 157) {
			return PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		} else if (channel == 165) {
			return PHY_SINGLE_CHANNEL_CENTERED;
		}
	}
	return PHY_SINGLE_CHANNEL_CENTERED;
}

/* ANI peer station count management and associated actions */
void lim_util_count_sta_add(struct mac_context *mac, tpDphHashNode pSta,
		struct pe_session *pe_session);
void lim_util_count_sta_del(struct mac_context *mac, tpDphHashNode pSta,
		struct pe_session *pe_session);

uint8_t lim_get_ht_capability(struct mac_context *, uint32_t, struct pe_session *);
QDF_STATUS lim_tx_complete(void *context, qdf_nbuf_t buf, bool free);

QDF_STATUS lim_validate_delts_req(struct mac_context *mac,
		tpSirDeltsReq pDeltsReq,
		tSirMacAddr peerMacAddr,
		struct pe_session *pe_session);

void lim_pkt_free(struct mac_context *mac,
		eFrameType frmType, uint8_t *pBD, void *body);

void lim_get_b_dfrom_rx_packet(struct mac_context *mac, void *body, uint32_t **pBD);

/**
 * utils_power_xy() - calc result of base raised to power
 * @base: Base value
 * @power: Base raised to this Power value
 *
 * Given a base(X) and power(Y), this API will return
 * the result of base raised to power - (X ^ Y)
 *
 * Return: Result of X^Y
 *
 */
static inline uint32_t utils_power_xy(uint16_t base, uint16_t power)
{
	uint32_t result = 1, i;

	for (i = 0; i < power; i++)
		result *= base;

	return result;
}

QDF_STATUS lim_post_sm_state_update(struct mac_context *mac,
		uint16_t StaIdx,
		tSirMacHTMIMOPowerSaveState MIMOPSState,
		uint8_t *pPeerStaMac, uint8_t sessionId);

void lim_delete_sta_context(struct mac_context *mac, struct scheduler_msg *limMsg);
void lim_delete_dialogue_token_list(struct mac_context *mac);

/**
 * lim_add_channel_status_info() - store
 * chan status info into Global MAC structure
 * @p_mac: Pointer to Global MAC structure
 * @channel_stat: Pointer to chan status info reported by firmware
 * @channel_id: current channel id
 *
 * Return: None
 */
void lim_add_channel_status_info(struct mac_context *p_mac,
				 struct lim_channel_status *channel_stat,
				 uint8_t channel_id);
uint8_t lim_get_channel_from_beacon(struct mac_context *mac,
		tpSchBeaconStruct pBeacon);
tSirNwType lim_get_nw_type(struct mac_context *mac, uint8_t channelNum,
		uint32_t type, tpSchBeaconStruct pBeacon);

void lim_set_tspec_uapsd_mask_per_session(struct mac_context *mac,
		struct pe_session *pe_session,
		struct mac_ts_info *pTsInfo, uint32_t action);

void lim_handle_heart_beat_timeout_for_session(struct mac_context *mac,
		struct pe_session *pe_session);

/**
 * lim_process_add_sta_rsp() - process WDA_ADD_STA_RSP from WMA
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: msg from WMA
 *
 * Return: void
 */
void lim_process_add_sta_rsp(struct mac_context *mac_ctx,
			     struct scheduler_msg *msg);

void lim_update_beacon(struct mac_context *mac);

void lim_process_ap_mlm_add_sta_rsp(struct mac_context *mac,
				    struct scheduler_msg *limMsgQ,
				    struct pe_session *pe_session);
void lim_process_ap_mlm_del_bss_rsp(struct mac_context *mac,
		struct scheduler_msg *limMsgQ,
		struct pe_session *pe_session);

void lim_process_ap_mlm_del_sta_rsp(struct mac_context *mac,
		struct scheduler_msg *limMsgQ,
		struct pe_session *pe_session);

struct pe_session *lim_is_ibss_session_active(struct mac_context *mac);
struct pe_session *lim_is_ap_session_active(struct mac_context *mac);
void lim_handle_heart_beat_failure_timeout(struct mac_context *mac);

#define limGetWscIEPtr(mac, ie, ie_len) \
	wlan_get_vendor_ie_ptr_from_oui(SIR_MAC_WSC_OUI, \
			SIR_MAC_WSC_OUI_SIZE, ie, ie_len)

#define limGetP2pIEPtr(mac, ie, ie_len) \
	wlan_get_vendor_ie_ptr_from_oui(SIR_MAC_P2P_OUI, \
			SIR_MAC_P2P_OUI_SIZE, ie, ie_len)

uint8_t lim_get_noa_attr_stream(struct mac_context *mac, uint8_t *pNoaStream,
		struct pe_session *pe_session);

uint8_t lim_build_p2p_ie(struct mac_context *mac, uint8_t *ie, uint8_t *data,
		uint8_t ie_len);

bool lim_isconnected_on_dfs_channel(struct mac_context *mac_ctx,
				    uint8_t currentChannel);

uint32_t lim_get_max_rate_flags(struct mac_context *mac_ctx,
				tpDphHashNode sta_ds);

bool lim_check_vht_op_mode_change(struct mac_context *mac,
		struct pe_session *pe_session,
		uint8_t chanWidth, uint8_t staId,
		uint8_t *peerMac);
#ifdef WLAN_FEATURE_11AX_BSS_COLOR
bool lim_send_he_ie_update(struct mac_context *mac_ctx, struct pe_session *pe_session);
#endif
bool lim_set_nss_change(struct mac_context *mac, struct pe_session *pe_session,
		uint8_t rxNss, uint8_t staId, uint8_t *peerMac);
bool lim_check_membership_user_position(struct mac_context *mac,
		struct pe_session *pe_session,
		uint32_t membership, uint32_t userPosition,
		uint8_t staId);

/**
 * enum ack_status - Indicate TX status of ASSOC/AUTH
 * @ACKED : Ack is received.
 * @NOT_ACKED : No Ack received.
 * @SENT_FAIL : Failure while sending.
 *
 * Indicate if driver is waiting for ACK status of assoc/auth or ACK received
 * for ASSOC/AUTH OR NO ACK is received for the assoc/auth sent or assoc/auth
 * sent failed.
 */
enum assoc_ack_status {
	ACKED,
	NOT_ACKED,
	SENT_FAIL,
};

typedef enum {
	WLAN_PE_DIAG_SCAN_REQ_EVENT = 0,
	WLAN_PE_DIAG_SCAN_ABORT_IND_EVENT,
	WLAN_PE_DIAG_SCAN_RSP_EVENT,
	WLAN_PE_DIAG_JOIN_REQ_EVENT,
	WLAN_PE_DIAG_JOIN_RSP_EVENT,
	WLAN_PE_DIAG_SETCONTEXT_REQ_EVENT,
	WLAN_PE_DIAG_SETCONTEXT_RSP_EVENT,
	WLAN_PE_DIAG_REASSOC_REQ_EVENT,
	WLAN_PE_DIAG_REASSOC_RSP_EVENT,
	WLAN_PE_DIAG_AUTH_REQ_EVENT,
	WLAN_PE_DIAG_AUTH_RSP_EVENT = 10,
	WLAN_PE_DIAG_DISASSOC_REQ_EVENT,
	WLAN_PE_DIAG_DISASSOC_RSP_EVENT,
	WLAN_PE_DIAG_DISASSOC_IND_EVENT,
	WLAN_PE_DIAG_DISASSOC_CNF_EVENT,
	WLAN_PE_DIAG_DEAUTH_REQ_EVENT,
	WLAN_PE_DIAG_DEAUTH_RSP_EVENT,
	WLAN_PE_DIAG_DEAUTH_IND_EVENT,
	WLAN_PE_DIAG_START_BSS_REQ_EVENT,
	WLAN_PE_DIAG_START_BSS_RSP_EVENT,
	WLAN_PE_DIAG_AUTH_IND_EVENT = 20,
	WLAN_PE_DIAG_ASSOC_IND_EVENT,
	WLAN_PE_DIAG_ASSOC_CNF_EVENT,
	WLAN_PE_DIAG_REASSOC_IND_EVENT,
	WLAN_PE_DIAG_SWITCH_CHL_IND_EVENT,
	WLAN_PE_DIAG_SWITCH_CHL_RSP_EVENT,
	WLAN_PE_DIAG_STOP_BSS_REQ_EVENT,
	WLAN_PE_DIAG_STOP_BSS_RSP_EVENT,
	WLAN_PE_DIAG_DEAUTH_CNF_EVENT,
	WLAN_PE_DIAG_ADDTS_REQ_EVENT,
	WLAN_PE_DIAG_ADDTS_RSP_EVENT = 30,
	WLAN_PE_DIAG_DELTS_REQ_EVENT,
	WLAN_PE_DIAG_DELTS_RSP_EVENT,
	WLAN_PE_DIAG_DELTS_IND_EVENT,
	WLAN_PE_DIAG_ENTER_BMPS_REQ_EVENT,
	WLAN_PE_DIAG_ENTER_BMPS_RSP_EVENT,
	WLAN_PE_DIAG_EXIT_BMPS_REQ_EVENT,
	WLAN_PE_DIAG_EXIT_BMPS_RSP_EVENT,
	WLAN_PE_DIAG_EXIT_BMPS_IND_EVENT,
	WLAN_PE_DIAG_ENTER_IMPS_REQ_EVENT,
	WLAN_PE_DIAG_ENTER_IMPS_RSP_EVENT = 40,
	WLAN_PE_DIAG_EXIT_IMPS_REQ_EVENT,
	WLAN_PE_DIAG_EXIT_IMPS_RSP_EVENT,
	WLAN_PE_DIAG_ENTER_UAPSD_REQ_EVENT,
	WLAN_PE_DIAG_ENTER_UAPSD_RSP_EVENT,
	WLAN_PE_DIAG_EXIT_UAPSD_REQ_EVENT,
	WLAN_PE_DIAG_EXIT_UAPSD_RSP_EVENT,
	WLAN_PE_DIAG_WOWL_ADD_BCAST_PTRN_EVENT,
	WLAN_PE_DIAG_WOWL_DEL_BCAST_PTRN_EVENT,
	WLAN_PE_DIAG_ENTER_WOWL_REQ_EVENT,
	WLAN_PE_DIAG_ENTER_WOWL_RSP_EVENT = 50,
	WLAN_PE_DIAG_EXIT_WOWL_REQ_EVENT,
	WLAN_PE_DIAG_EXIT_WOWL_RSP_EVENT,
	WLAN_PE_DIAG_HAL_ADDBA_REQ_EVENT,
	WLAN_PE_DIAG_HAL_ADDBA_RSP_EVENT,
	WLAN_PE_DIAG_HAL_DELBA_IND_EVENT,
	WLAN_PE_DIAG_HB_FAILURE_TIMEOUT,
	WLAN_PE_DIAG_PRE_AUTH_REQ_EVENT,
	WLAN_PE_DIAG_PRE_AUTH_RSP_EVENT,
	WLAN_PE_DIAG_PREAUTH_DONE,
	WLAN_PE_DIAG_REASSOCIATING = 60,
	WLAN_PE_DIAG_CONNECTED,
	WLAN_PE_DIAG_ASSOC_REQ_EVENT,
	WLAN_PE_DIAG_AUTH_COMP_EVENT,
	WLAN_PE_DIAG_ASSOC_COMP_EVENT,
	WLAN_PE_DIAG_AUTH_START_EVENT,
	WLAN_PE_DIAG_ASSOC_START_EVENT,
	WLAN_PE_DIAG_REASSOC_START_EVENT,
	WLAN_PE_DIAG_ROAM_AUTH_START_EVENT,
	WLAN_PE_DIAG_ROAM_AUTH_COMP_EVENT,
	WLAN_PE_DIAG_ROAM_ASSOC_START_EVENT = 70,
	WLAN_PE_DIAG_ROAM_ASSOC_COMP_EVENT,
	WLAN_PE_DIAG_SCAN_COMPLETE_EVENT,
	WLAN_PE_DIAG_SCAN_RESULT_FOUND_EVENT,
	WLAN_PE_DIAG_ASSOC_TIMEOUT,
	WLAN_PE_DIAG_AUTH_TIMEOUT,
	WLAN_PE_DIAG_DEAUTH_FRAME_EVENT,
	WLAN_PE_DIAG_DISASSOC_FRAME_EVENT,
	WLAN_PE_DIAG_AUTH_ACK_EVENT,
	WLAN_PE_DIAG_ASSOC_ACK_EVENT,
	WLAN_PE_DIAG_AUTH_ALGO_NUM,
} WLAN_PE_DIAG_EVENT_TYPE;

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void lim_diag_event_report(struct mac_context *mac, uint16_t eventType,
		struct pe_session *pe_session, uint16_t status,
		uint16_t reasonCode);
/**
 * lim_diag_mgmt_tx_event_report() - to log TX event to external application
 * @mac_ctx: mac context
 * @mgmt_hdr: 802.11 mgmt header of given frame
 * @session: PE session for given frame
 * @result_code: result code of to be populated in TX frame
 * @reason_code: reason code if TX OTA status
 *
 * Anytime driver sends some mgmt frame down to firmware for OTA delivery,
 * log mgmt frame through DIAG utility. Don't log frames which come too
 * excessively.
 *
 * Return: void
 */
void lim_diag_mgmt_tx_event_report(struct mac_context *mac_ctx, void *mgmt_hdr,
		struct pe_session *session, uint16_t result_code,
		uint16_t reason_code);
/**
 * lim_diag_mgmt_rx_event_report() - to log RX event to external application
 * @mac_ctx: mac context
 * @mgmt_hdr: 802.11 mgmt header of given frame
 * @session: PE session for given frame
 * @result_code: result code given in RX frame
 * @reason_code: reason code for RX OTA status
 *
 * Anytime driver receives some mgmt frame from firmware OTA,
 * log mgmt frame through DIAG utility. Don't log frames which come too
 * excessively.
 *
 * Return: void
 */
void lim_diag_mgmt_rx_event_report(struct mac_context *mac_ctx, void *mgmt_hdr,
		struct pe_session *session, uint16_t result_code,
		uint16_t reason_code);
#else
static inline void lim_diag_event_report(struct mac_context *mac, uint16_t
		eventType, struct pe_session *pe_session, uint16_t status,
		uint16_t reasonCode) {}
static inline
void lim_diag_mgmt_tx_event_report(struct mac_context *mac_ctx, void *mgmt_hdr,
		struct pe_session *session, uint16_t result_code,
		uint16_t reason_code) {}
static inline
void lim_diag_mgmt_rx_event_report(struct mac_context *mac_ctx, void *mgmt_hdr,
		struct pe_session *session, uint16_t result_code,
		uint16_t reason_code) {}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

void pe_set_resume_channel(struct mac_context *mac, uint16_t channel,
		ePhyChanBondState cbState);

void lim_get_short_slot_from_phy_mode(struct mac_context *mac,
		struct pe_session *pe_session,
		uint32_t phyMode, uint8_t *pShortSlotEnable);

void lim_clean_up_disassoc_deauth_req(struct mac_context *mac, uint8_t *staMac,
		bool cleanRxPath);

bool lim_check_disassoc_deauth_ack_pending(struct mac_context *mac,
		uint8_t *staMac);

#ifdef WLAN_FEATURE_11W
void lim_pmf_sa_query_timer_handler(void *pMacGlobal, uint32_t param);
void lim_set_protected_bit(struct mac_context *mac,
	struct pe_session *pe_session,
	tSirMacAddr peer, tpSirMacMgmtHdr pMacHdr);
#else
static inline void lim_set_protected_bit(struct mac_context *mac,
	struct pe_session *pe_session,
	tSirMacAddr peer, tpSirMacMgmtHdr pMacHdr) {}
#endif /* WLAN_FEATURE_11W */

void lim_set_ht_caps(struct mac_context *p_mac,
		struct pe_session *p_session_ntry,
		uint8_t *p_ie_start,
		uint32_t num_bytes);

void lim_set_vht_caps(struct mac_context *p_mac,
		struct pe_session *p_session_entry,
		uint8_t *p_ie_start,
		uint32_t num_bytes);
bool lim_validate_received_frame_a1_addr(struct mac_context *mac_ctx,
		tSirMacAddr a1, struct pe_session *session);
void lim_set_stads_rtt_cap(tpDphHashNode sta_ds, struct s_ext_cap *ext_cap,
			   struct mac_context *mac_ctx);

void lim_check_and_reset_protection_params(struct mac_context *mac_ctx);

QDF_STATUS lim_send_ext_cap_ie(struct mac_context *mac_ctx, uint32_t session_id,
			       tDot11fIEExtCap *extracted_extcap, bool merge);

QDF_STATUS lim_send_ies_per_band(struct mac_context *mac_ctx,
				 struct pe_session *session, uint8_t vdev_id);

/**
 * lim_send_action_frm_tb_ppdu_cfg() - sets action frame in TB PPDU cfg to FW
 * @mac_ctx: global MAC context
 * @session_id: SME session id
 * @cfg: config setting
 *
 * Preapres the vendor action frame and send action frame in HE TB PPDU
 * configuration to FW.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_send_action_frm_tb_ppdu_cfg(struct mac_context *mac_ctx,
					   uint32_t session_id,
					   uint8_t cfg);

void lim_update_extcap_struct(struct mac_context *mac_ctx, uint8_t *buf,
			      tDot11fIEExtCap *ext_cap);
QDF_STATUS lim_strip_extcap_update_struct(struct mac_context *mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen, tDot11fIEExtCap *dst);
void lim_merge_extcap_struct(tDot11fIEExtCap *dst, tDot11fIEExtCap *src,
		bool add);

#ifdef WLAN_FEATURE_11W
/**
 * lim_del_pmf_sa_query_timer() - This function deletes SA query timer
 * @mac_ctx: pointer to mac context
 * @pe_session: pointer to PE session
 *
 * This API is to delete the PMF SA query timer created for each associated STA
 *
 * Return: none
 */
void lim_del_pmf_sa_query_timer(struct mac_context *mac_ctx, struct pe_session *pe_session);
#else
/**
 * lim_del_pmf_sa_query_timer() - This function deletes SA query timer
 * @mac_ctx: pointer to mac context
 * @pe_session: pointer to PE session
 *
 * This API is to delete the PMF SA query timer created for each associated STA
 *
 * Return: none
 */
static inline void
lim_del_pmf_sa_query_timer(struct mac_context *mac_ctx, struct pe_session *pe_session)
{
}
#endif

/**
 * lim_add_bssid_to_reject_list:- Add rssi reject Ap info to blacklist mgr.
 * @pdev: pdev
 * @entry: info of the BSSID to be put in rssi reject list.
 *
 * This API will add the passed ap info to the rssi reject list.
 *
 */
void
lim_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
			     struct sir_rssi_disallow_lst *entry);

/**
 * lim_strip_op_class_update_struct - strip sup op class IE and populate
 *				  the dot11f structure
 * @mac_ctx: global MAC context
 * @addn_ie: Additional IE buffer
 * @addn_ielen: Length of additional IE
 * @dst: Supp operating class IE structure to be updated
 *
 * This function is used to strip supp op class IE from IE buffer and
 * update the passed structure.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_strip_supp_op_class_update_struct(struct mac_context *mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen,
		tDot11fIESuppOperatingClasses *dst);

uint8_t lim_get_80Mhz_center_channel(uint8_t primary_channel);
void lim_update_obss_scanparams(struct pe_session *session,
			tDot11fIEOBSSScanParameters *scan_params);
void lim_init_obss_params(struct mac_context *mac_ctx, struct pe_session *session);
#ifdef WLAN_FEATURE_HOST_ROAM
uint32_t lim_create_timers_host_roam(struct mac_context *mac_ctx);
/**
 * lim_delete_timers_host_roam() - Delete timers used in host based roaming
 * @mac_ctx: Global MAC context
 *
 * Delete reassoc and preauth timers
 *
 * Return: none
 */
void lim_delete_timers_host_roam(struct mac_context *mac_ctx);
/**
 * lim_deactivate_timers_host_roam() - deactivate timers used in host based
 * roaming
 * @mac_ctx: Global MAC context
 *
 * Delete reassoc and preauth timers
 *
 * Return: none
 */
void lim_deactivate_timers_host_roam(struct mac_context *mac_ctx);
void lim_deactivate_and_change_timer_host_roam(struct mac_context *mac_ctx,
		uint32_t timer_id);
#else
static inline uint32_t lim_create_timers_host_roam(struct mac_context *mac_ctx)
{
	return 0;
}
static inline void lim_delete_timers_host_roam(struct mac_context *mac_ctx)
{}
static inline void lim_deactivate_timers_host_roam(struct mac_context *mac_ctx) {}
static inline void lim_deactivate_and_change_timer_host_roam(
		struct mac_context *mac_ctx, uint32_t timer_id)
{}
#endif

bool lim_is_robust_mgmt_action_frame(uint8_t action_category);
uint8_t lim_compute_ext_cap_ie_length(tDot11fIEExtCap *ext_cap);

void lim_update_caps_info_for_bss(struct mac_context *mac_ctx,
			uint16_t *caps, uint16_t bss_caps);
void lim_send_set_dtim_period(struct mac_context *mac_ctx, uint8_t dtim_period,
			      struct pe_session *session);

QDF_STATUS lim_strip_ie(struct mac_context *mac_ctx,
		uint8_t *addn_ie, uint16_t *addn_ielen,
		uint8_t eid, eSizeOfLenField size_of_len_field,
		uint8_t *oui, uint8_t out_len, uint8_t *extracted_ie,
		uint32_t eid_max_len);

#define MCSMAPMASK1x1 0x3
#define MCSMAPMASK2x2 0xC

#ifdef WLAN_FEATURE_11AX

/**
 * lim_intersect_ap_he_caps() - Intersect AP capability with self STA capability
 * @session: pointer to PE session
 * @add_bss: pointer to ADD BSS params
 * @beacon: pointer to beacon
 * @assoc_rsp: pointer to assoc response
 *
 * Return: None
 */
void lim_intersect_ap_he_caps(struct pe_session *session, tpAddBssParams add_bss,
		tSchBeaconStruct *pBeaconStruct, tpSirAssocRsp assoc_rsp);

/**
 * lim_intersect_sta_he_caps() - Intersect STA capability with SAP capability
 * @assoc_req: pointer to assoc request
 * @session: pointer to PE session
 * @sta_ds: pointer to STA dph hash table entry
 *
 * Return: None
 */
void lim_intersect_sta_he_caps(tpSirAssocReq assoc_req, struct pe_session *session,
		tpDphHashNode sta_ds);

/**
 * lim_add_he_cap() - Copy HE capability into Add sta params
 * @add_sta_params: pointer to add sta params
 * @assoc_req: pointer to Assoc request
 *
 * Return: None
 */
void lim_add_he_cap(tpAddStaParams add_sta_params, tpSirAssocReq assoc_req);

/**
 * lim_add_self_he_cap() - Copy HE capability into add sta from PE session
 * @add_sta_params: pointer to add sta params
 * @session: pointer to PE Session
 *
 * Return: None
 */
void lim_add_self_he_cap(tpAddStaParams add_sta_params, struct pe_session *session);

/**
 * lim_add_bss_he_cap() - Copy HE capability into ADD BSS params
 * @add_bss: pointer to add bss params
 * @assoc_rsp: pointer to assoc response
 *
 * Return: None
 */
void lim_add_bss_he_cap(tpAddBssParams add_bss, tpSirAssocRsp assoc_rsp);

/**
 * lim_add_bss_he_cfg() - Set HE config to BSS params
 * @add_bss: pointer to add bss params
 * @session: Pointer to Session entry struct
 *
 * Return: None
 */
void lim_add_bss_he_cfg(tpAddBssParams add_bss, struct pe_session *session);

/**
 * lim_copy_bss_he_cap() - Copy HE capability into PE session from start bss
 * @session: pointer to PE session
 * @sme_start_bss_req: pointer to start BSS request
 *
 * Return: None
 */
void lim_copy_bss_he_cap(struct pe_session *session,
			 struct start_bss_req *sme_start_bss_req);

/**
 * lim_copy_join_req_he_cap() - Copy HE capability to PE session from Join req
 * and update as per bandwidth supported
 * @session: pointer to PE session
 * @sme_join_req: pointer to SME join request
 *
 * Return: None
 */
void lim_copy_join_req_he_cap(struct pe_session *session,
			      struct join_req *sme_join_req);

/**
 * lim_log_he_op() - Print HE Operation
 * @mac: pointer to MAC context
 * @he_op: pointer to HE Operation
 *
 * Print HE operation stored as dot11f structure
 *
 * Return: None
 */
void lim_log_he_op(struct mac_context *mac, tDot11fIEhe_op *he_ops);

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
/**
 * lim_log_he_bss_color() - Print HE bss color
 * @mac: pointer to MAC context
 * @he_bss_color: pointer to HE bss color
 *
 * Print HE bss color IE
 *
 * Return: None
 */
void lim_log_he_bss_color(struct mac_context *mac,
			tDot11fIEbss_color_change *he_bss_color);
#endif

/**
 * lim_log_he_cap() - Print HE capabilities
 * @mac: pointer to MAC context
 * @he_cap: pointer to HE Capability
 *
 * Received HE capabilities are converted into dot11f structure.
 * This function will print all the HE capabilities as stored
 * in the dot11f structure.
 *
 * Return: None
 */
void lim_log_he_cap(struct mac_context *mac, tDot11fIEhe_cap *he_cap);

/**
 * lim_update_stads_he_caps() - Copy HE capability into STA DPH hash table entry
 * @sta_ds: pointer to sta dph hash table entry
 * @assoc_rsp: pointer to assoc response
 * @session_entry: pointer to PE session
 *
 * Return: None
 */
void lim_update_stads_he_caps(tpDphHashNode sta_ds, tpSirAssocRsp assoc_rsp,
			      struct pe_session *session_entry);

/**
 * lim_update_usr_he_cap() - Update HE capability based on userspace
 * @mac_ctx: global mac context
 * @session: PE session entry
 *
 * Parse the HE Capability IE and populate the fields to be
 * sent to FW as part of add bss and update PE session.
 */
void lim_update_usr_he_cap(struct mac_context *mac_ctx, struct pe_session *session);

/**
 * lim_decide_he_op() - Determine HE operation elements
 * @mac_ctx: global mac context
 * @he_ops: pointer to HE operation IE
 * @session: PE session entry
 *
 * Parse the HE Operation IE and populate the fields to be
 * sent to FW as part of add bss.
 */
void lim_decide_he_op(struct mac_context *mac_ctx, tpAddBssParams add_bss,
		struct pe_session *session);

/**
 * lim_update_sta_he_capable(): Update he_capable in add sta params
 * @mac: pointer to MAC context
 * @add_sta_params: pointer to add sta params
 * @sta_ds: pointer to dph hash table entry
 * @session_entry: pointer to PE session
 *
 * Return: None
 */
void lim_update_sta_he_capable(struct mac_context *mac,
	tpAddStaParams add_sta_params, tpDphHashNode sta_ds,
	struct pe_session *session_entry);

static inline bool lim_is_session_he_capable(struct pe_session *session)
{
	return session->he_capable;
}

/**
 * lim_get_session_he_frag_cap(): Get session HE fragmentation cap
 * @session: pointer to session
 *
 * Return: HE fragmentation value
 */
static inline uint8_t lim_get_session_he_frag_cap(struct pe_session *session)
{
	return session->he_config.fragmentation;
}

static inline bool lim_is_sta_he_capable(tpDphHashNode sta_ds)
{
	return sta_ds->mlmStaContext.he_capable;
}

/**
 * lim_update_bss_he_capable(): Update he_capable in add BSS params
 * @mac: pointer to MAC context
 * @add_bss: pointer to add BSS params
 *
 * Return: None
 */
void lim_update_bss_he_capable(struct mac_context *mac, tpAddBssParams add_bss);

/**
 * lim_update_stads_he_capable() - Update he_capable in sta ds context
 * @sta_ds: pointer to sta ds
 * @assoc_req: pointer to assoc request
 *
 * Return: None
 */
void lim_update_stads_he_capable(tpDphHashNode sta_ds, tpSirAssocReq assoc_req);

/**
 * lim_update_session_he_capable(): Update he_capable in PE session
 * @mac: pointer to MAC context
 * @session: pointer to PE session
 *
 * Return: None
 */
void lim_update_session_he_capable(struct mac_context *mac, struct pe_session *session);

/**
 * lim_update_chan_he_capable(): Update he_capable in chan switch params
 * @mac: pointer to MAC context
 * @chan: pointer to channel switch params
 *
 * Return: None
 */
void lim_update_chan_he_capable(struct mac_context *mac, tpSwitchChannelParams chan);

/**
 * lim_set_he_caps() - update HE caps to be sent to FW as part of scan IE
 * @mac: pointer to MAC
 * @session: pointer to PE session
 * @ie_start: pointer to start of IE buffer
 * @num_bytes: length of IE buffer
 *
 * Return: None
 */
void lim_set_he_caps(struct mac_context *mac, struct pe_session *session,
		     uint8_t *ie_start, uint32_t num_bytes);

/**
 * lim_send_he_caps_ie() - gets HE capability and send to firmware via wma
 * @mac_ctx: global mac context
 * @session: pe session. This can be NULL. In that case self cap will be sent
 * @vdev_id: vdev for which IE is targeted
 *
 * This function gets HE capability and send to firmware via wma
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_send_he_caps_ie(struct mac_context *mac_ctx, struct pe_session *session,
			       uint8_t vdev_id);

/**
 * lim_populate_he_mcs_set() - function to populate HE mcs rate set
 * @mac_ctx: pointer to global mac structure
 * @rates: pointer to supported rate set
 * @peer_vht_caps: pointer to peer HE capabilities
 * @session_entry: pe session entry
 * @nss: number of spatial streams
 *
 * Populates HE mcs rate set based on peer and self capabilities
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_populate_he_mcs_set(struct mac_context *mac_ctx,
				   struct supported_rates *rates,
				   tDot11fIEhe_cap *peer_he_caps,
				   struct pe_session *session_entry,
				   uint8_t nss);

#else
static inline void lim_add_he_cap(tpAddStaParams add_sta_params,
				  tpSirAssocReq assoc_req)
{
}

static inline void lim_add_self_he_cap(tpAddStaParams add_sta_params,
				       struct pe_session *session)
{
}

static inline void lim_add_bss_he_cap(tpAddBssParams add_bss,
				      tpSirAssocRsp assoc_rsp)
{
	return;
}

static inline void lim_add_bss_he_cfg(tpAddBssParams add_bss,
					 struct pe_session *session)
{
}

static inline void lim_intersect_ap_he_caps(struct pe_session *session,
		tpAddBssParams add_bss,	tSchBeaconStruct *pBeaconStruct,
		tpSirAssocRsp assoc_rsp)
{
	return;
}

static inline void lim_intersect_sta_he_caps(tpSirAssocReq assoc_req,
		struct pe_session *session, tpDphHashNode sta_ds)
{
}

static inline void lim_update_stads_he_caps(tpDphHashNode sta_ds, tpSirAssocRsp assoc_rsp,
		struct pe_session *session_entry)
{
	return;
}

static inline void lim_update_usr_he_cap(struct mac_context *mac_ctx,
			struct pe_session *session)
{
}

static inline void lim_decide_he_op(struct mac_context *mac_ctx,
			tpAddBssParams add_bss, struct pe_session *session)
{
}

static inline
void lim_copy_bss_he_cap(struct pe_session *session,
			 struct start_bss_req *sme_start_bss_req)
{
}

static inline void lim_copy_join_req_he_cap(struct pe_session *session,
			struct join_req *sme_join_req)
{
}

static inline void lim_log_he_op(struct mac_context *mac,
	tDot11fIEhe_op *he_ops)
{
}

static inline void lim_log_he_cap(struct mac_context *mac,
	tDot11fIEhe_cap *he_cap)
{
}

static inline void lim_update_sta_he_capable(struct mac_context *mac,
			tpAddStaParams add_sta_params,
			tpDphHashNode sta_ds, struct pe_session *session_entry)
{
}

static inline bool lim_is_session_he_capable(struct pe_session *session)
{
	return false;
}

static inline uint8_t lim_get_session_he_frag_cap(struct pe_session *session)
{
	return 0;
}

static inline bool lim_is_sta_he_capable(tpDphHashNode sta_ds)
{
	return false;
}

static inline void lim_update_bss_he_capable(struct mac_context *mac,
			tpAddBssParams add_bss)
{
}

static inline void lim_update_stads_he_capable(tpDphHashNode sta_ds,
		tpSirAssocReq assoc_req)
{
}

static inline void lim_update_session_he_capable(struct mac_context *mac,
			struct pe_session *session)
{
}

static inline void lim_update_chan_he_capable(struct mac_context *mac,
		tpSwitchChannelParams chan)
{
}

static inline void lim_set_he_caps(struct mac_context *mac, struct pe_session *session,
				   uint8_t *ie_start, uint32_t num_bytes)
{
}

static inline QDF_STATUS lim_send_he_caps_ie(struct mac_context *mac_ctx,
					     struct pe_session *session,
					     uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS lim_populate_he_mcs_set(struct mac_context *mac_ctx,
				   struct supported_rates *rates,
				   tDot11fIEhe_cap *peer_he_caps,
				   struct pe_session *session_entry,
				   uint8_t nss)
{
	return QDF_STATUS_SUCCESS;
}

#endif

/**
 * lim_decrement_pending_mgmt_count: Decrement mgmt frame count
 * @mac_ctx: Pointer to global MAC structure
 *
 * This function is used to decrement pe mgmt count once frame
 * removed from queue
 *
 * Return: None
 */
void lim_decrement_pending_mgmt_count(struct mac_context *mac_ctx);

/**
 * lim_check_if_vendor_oui_match() - Check if the given OUI match in IE buffer
 * @mac_ctx: MAC context
 * @ie: IE buffer
 * @ie_len: length of @ie
 *
 * This API is used to check if given vendor OUI
 * matches in given IE buffer
 *
 * Return: True, if mataches. False otherwise
 */
bool lim_check_if_vendor_oui_match(struct mac_context *mac_ctx,
					uint8_t *oui, uint8_t oui_len,
			       uint8_t *ie, uint8_t ie_len);

QDF_STATUS lim_util_get_type_subtype(void *pkt, uint8_t *type,
					uint8_t *subtype);

/**
 * lim_get_min_session_txrate() - Get the minimum rate supported in the session
 * @session: Pointer to PE session
 *
 * This API will find the minimum rate supported by the given PE session and
 * return the enum rateid corresponding to the rate.
 *
 * Return: enum rateid
 */
enum rateid lim_get_min_session_txrate(struct pe_session *session);

/**
 * lim_send_dfs_chan_sw_ie_update() - updates the channel switch IE in beacon
 * template
 *
 * @mac_ctx - pointer to global mac context
 * @session - A pointer to pesession
 * Return None
 */
void lim_send_dfs_chan_sw_ie_update(struct mac_context *mac_ctx,
				    struct pe_session *session);

/**
 * lim_process_ap_ecsa_timeout() -process ECSA timeout which decrement csa count
 * in beacon and update beacon template in firmware
 *
 * @data - A pointer to pesession
 * Return None
 */
void lim_process_ap_ecsa_timeout(void *session);

/**
 * lim_send_stop_bss_failure_resp() -send failure delete bss resp to sme
 * @mac_ctx: mac ctx
 * @session: session pointer
 *
 * Return None
 */
void lim_send_stop_bss_failure_resp(struct mac_context *mac_ctx,
				    struct pe_session *session);

/**
 * lim_delete_all_peers() -delete all connected peers
 * @session: session pointer
 *
 * Return None
 */
void lim_delete_all_peers(struct pe_session *session);

/**
 * lim_send_vdev_stop() -send delete bss/stop vdev req
 * @session: session pointer
 *
 * Return QDF_STATUS
 */
QDF_STATUS lim_send_vdev_stop(struct pe_session *session);

/**
 * lim_send_vdev_stop() -send delete bss/stop vdev req for STA
 * @session: session pointer
 *
 * Return QDF_STATUS
 */
QDF_STATUS lim_sta_send_del_bss(struct pe_session *session);

/**
 * lim_send_start_bss_confirm() -send start bss confirm req
 * @mac_ctx: pointer to global mac structure
 * @start_cnf: start confirm structure pointer
 *
 * Return None
 */
void lim_send_start_bss_confirm(struct mac_context *mac_ctx,
				     tLimMlmStartCnf *start_cnf);

/**
 * lim_send_chan_switch_action_frame()- Send an action frame
 * containing CSA IE or ECSA IE depending on the connected
 * sta capability.
 *
 * @mac_ctx: pointer to global mac structure
 * @new_channel: new channel to switch to.
 * @ch_bandwidth: BW of channel to calculate op_class
 * @session_entry: pe session
 *
 * Return: void
 */
void lim_send_chan_switch_action_frame(struct mac_context *mac_ctx,
				       uint16_t new_channel,
				       uint8_t ch_bandwidth,
				       struct pe_session *session_entry);

/**
 * lim_process_obss_detection_ind() - Process obss detection indication
 * @mac_ctx: Pointer to Global MAC structure.
 * @obss_detection: obss detection info.
 *
 * Process obss detection indication and apply necessary protection for
 * the given AP session.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_process_obss_detection_ind(struct mac_context *mac_ctx,
					  struct wmi_obss_detect_info
					  *obss_detection);

/**
 * lim_obss_send_detection_cfg() - Send obss detection configuration to firmware
 * @mac_ctx: Pointer to Global MAC structure
 * @session: Pointer to session
 * @force: Force to send new configuration even if new cfg same as old
 *
 * Generate new cfg based on current protection status and send new cfg to
 * firmware.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_obss_send_detection_cfg(struct mac_context *mac_ctx,
				       struct pe_session *session,
				       bool force);

/**
 * lim_obss_generate_detection_config() - get new obss offload detection cfg
 * @mac_ctx: Pointer to Global MAC structure
 * @session: Pointer to session
 * @cfg: Obss detection cfg buffer pointer
 *
 * Generate new cfg based on current protection status.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_obss_generate_detection_config(struct mac_context *mac_ctx,
					      struct pe_session *session,
					      struct obss_detection_cfg *cfg);

/**
 * lim_enable_obss_detection_config() - Enable obss detection
 * @mac_ctx: Pointer to Global MAC structure
 * @session: Pointer to session
 *
 * This function will enable legacy obss detection (by starting timer)
 * or also offload based detection based on support.
 *
 * Return: None
 */
void lim_enable_obss_detection_config(struct mac_context *mac_ctx,
				      struct pe_session *session);

#ifdef WLAN_SUPPORT_TWT
void lim_set_peer_twt_cap(struct pe_session *session, struct s_ext_cap *ext_cap);
#else
static inline void lim_set_peer_twt_cap(struct pe_session *session,
					struct s_ext_cap *ext_cap)
{
}
#endif

/**
 * lim_rx_invalid_peer_process() - process rx invalid peer indication
 * @mac_ctx: Pointer to Global MAC structure
 * @lim_msg: Pointer to scheduler message
 *
 * This function will process the rx data invalid peer indication,
 * if the vdev operation mode is SAP, then send the deauth mgmt frame
 * to STA.
 *
 * Return: None
 */
void lim_rx_invalid_peer_process(struct mac_context *mac_ctx,
				 struct scheduler_msg *lim_msg);

/**
 * lim_send_beacon() - send beacon indication to firmware
 * @mac_ctx: Pointer to Global MAC structure
 * @session: session pointer
 *
 * Return: None
 */
void lim_send_beacon(struct mac_context *mac_ctx, struct pe_session *session);

/**
 * lim_ndi_mlme_vdev_up_transition() - Send event to transistion NDI VDEV to UP
 * @session: session pointer
 *
 * Return: None
 */
void lim_ndi_mlme_vdev_up_transition(struct pe_session *session);

/**
 * lim_sap_move_to_cac_wait_state() - move to cac wait state
 * @sap_ctx: SAP context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_sap_move_to_cac_wait_state(struct pe_session *session);

/**
 * lim_disconnect_complete - Deliver vdev disconnect complete event or
 * STA send deleting bss
 * @session: PE session pointer
 * @del_bss:  Whether to call lim_sta_send_del_bss
 *
 * API delivers vdev disconnect complete event
 *
 * Return: None
 */
void lim_disconnect_complete(struct pe_session *session, bool del_bss);

/**
 * lim_sta_mlme_vdev_stop_send() - send VDEV stop
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes vdev stop
 *
 * Return: SUCCESS on successful completion of vdev stop
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_sta_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data);

/**
 * lim_sta_mlme_vdev_req_fail() - send VDEV start req failure
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes vdev stop
 *
 * Return: SUCCESS on successful completion of req failure operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_sta_mlme_vdev_req_fail(struct vdev_mlme_obj *vdev_mlme,
				      uint16_t data_len, void *data);

/**
 * lim_sta_mlme_vdev_start_send() - send VDEV start
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes vdev start
 *
 * Return: SUCCESS on successful completion of vdev start
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_sta_mlme_vdev_start_send(struct vdev_mlme_obj *vdev_mlme,
					uint16_t data_len, void *data);

/**
 * lim_sta_mlme_vdev_restart_send() - send VDEV restart
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes vdev restart
 *
 * Return: SUCCESS on successful completion of vdev restart
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_sta_mlme_vdev_restart_send(struct vdev_mlme_obj *vdev_mlme,
					  uint16_t data_len, void *data);

/**
 * lim_ap_mlme_vdev_start_send() - Invokes VDEV start operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV start operation
 *
 * Return: SUCCESS on successful completion of start operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_ap_mlme_vdev_start_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *event);
/*
 * lim_ap_mlme_vdev_update_beacon() - Updates beacon
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @op: beacon update type
 * @data_len: data size
 * @data: event data
 *
 * API updates/allocates/frees the beacon
 *
 * Return: SUCCESS on successful update of beacon
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_ap_mlme_vdev_update_beacon(struct vdev_mlme_obj *vdev_mlme,
					  enum beacon_update_op op,
					  uint16_t data_len, void *data);

/**
 * lim_ap_mlme_vdev_up_send() - VDEV up operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV up operations
 *
 * Return: SUCCESS on successful completion of up operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_ap_mlme_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
				    uint16_t data_len, void *data);

/**
 * lim_ap_mlme_vdev_disconnect_peers - Disconnect peers
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API trigger stations disconnection connected with AP
 *
 * Return: SUCCESS on successful invocation of station disconnection
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_ap_mlme_vdev_disconnect_peers(struct vdev_mlme_obj *vdev_mlme,
					     uint16_t data_len, void *data);

/**
 * lim_ap_mlme_vdev_stop_send - Invokes VDEV stop operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV stop operation
 *
 * Return: SUCCESS on successful completion of stop operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_ap_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
				      uint16_t data_len, void *data);

/**
 * lim_ap_mlme_vdev_restart_send - Invokes VDEV restart operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV restart operation
 *
 * Return: SUCCESS on successful completion of restart operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_ap_mlme_vdev_restart_send(struct vdev_mlme_obj *vdev_mlme,
					 uint16_t data_len, void *data);

/**
 * lim_ap_mlme_vdev_start_req_failed - handle vdev start req failure
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes on START fail response
 *
 * Return: SUCCESS on successful invocation of callback
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_ap_mlme_vdev_start_req_failed(struct vdev_mlme_obj *vdev_mlme,
					     uint16_t data_len, void *data);

/**
 * lim_mon_mlme_vdev_start_send() - Invokes VDEV start operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV start operation
 *
 * Return: SUCCESS on successful completion of start operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_mon_mlme_vdev_start_send(struct vdev_mlme_obj *vdev_mlme,
					uint16_t data_len, void *event);

#ifdef CRYPTO_SET_KEY_CONVERGED
static inline bool lim_is_set_key_req_converged(void)
{
	return true;
}

static inline void lim_copy_set_key_req_mac_addr(struct qdf_mac_addr *dst,
						 struct qdf_mac_addr *src)
{
	qdf_copy_macaddr(dst, src);
}
#else
static inline bool lim_is_set_key_req_converged(void)
{
	return false;
}

static inline void lim_copy_set_key_req_mac_addr(struct qdf_mac_addr *dst,
						 struct qdf_mac_addr *src)
{
}
#endif

/**
 * lim_get_regulatory_max_transmit_power() - Get regulatory max transmit
 * power on given channel
 * @mac:     pointer to mac data
 * @channel: channel number
 *
 * Return:  int8_t - power
 */
int8_t lim_get_regulatory_max_transmit_power(struct mac_context *mac,
					     uint8_t channel);

/**
 * lim_get_capability_info() - Get capability information
 * @mac:        pointer to mac data
 * @pcap:       pointer to return capability information
 * @pe_session: pointer to pe session
 *
 * Return:  SUCCESS on successful get capability information
 *         FAILURE, if it fails due to any
 */
QDF_STATUS lim_get_capability_info(struct mac_context *mac, uint16_t *pCap,
				   struct pe_session *pe_session);

/**
 * lim_flush_bssid() - flush bssid from scan cache
 * @mac_ctx: pointer to mac data
 * @bssid: bssid to be flushed
 *
 * Return: void
 */
void lim_flush_bssid(struct mac_context *mac_ctx, uint8_t *bssid);

/**
 * lim_is_sha384_akm() - Function to check if the negotiated AKM for the
 * current session is based on sha384 key derivation function.
 * @mac_ctx: pointer to mac data
 * @akm: negotiated AKM for the current session
 *
 * Return: true if akm is sha384 based kdf or false
 */
bool lim_is_sha384_akm(enum ani_akm_type akm);

#endif /* __LIM_UTILS_H */

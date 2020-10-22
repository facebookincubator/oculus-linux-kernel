/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * DOC: csr_inside_api.h
 *
 * Define interface only used by CSR.
 */
#ifndef CSR_INSIDE_API_H__
#define CSR_INSIDE_API_H__

#include "csr_support.h"
#include "sme_inside.h"
#include "cds_reg_service.h"
#include "wlan_objmgr_vdev_obj.h"

/* This number minus 1 means the number of times a channel is scanned before
 * a BSS is removed from cache scan result
 */
#define CSR_AGING_COUNT     3

/* These are going against the signed RSSI (int8_t) so it is between -+127 */
#define CSR_BEST_RSSI_VALUE         (-30)       /* RSSI >= this is in CAT4 */
#define CSR_DEFAULT_RSSI_DB_GAP     30  /* every 30 dbm for one category */

#ifdef QCA_WIFI_3_0_EMU
#define CSR_ACTIVE_SCAN_LIST_CMD_TIMEOUT (1000*30*20)
#else
#define CSR_ACTIVE_SCAN_LIST_CMD_TIMEOUT (1000*30)
#endif
/* ***************************************************************************
 * The MAX BSSID Count should be lower than the command timeout value.
 * As in some case auth timeout can take upto 5 sec (in case of SAE auth) try
 * (command timeout/5000 - 1) candidates.
 * ***************************************************************************/
#define CSR_MAX_BSSID_COUNT     (SME_ACTIVE_LIST_CMD_TIMEOUT_VALUE/5000) - 1
#define CSR_CUSTOM_CONC_GO_BI    100
extern uint8_t csr_wpa_oui[][CSR_WPA_OUI_SIZE];
bool csr_is_supported_channel(struct mac_context *mac, uint8_t channelId);

enum csr_scancomplete_nextcommand {
	eCsrNextScanNothing,
	eCsrNexteScanForSsidSuccess,
	eCsrNexteScanForSsidFailure,
	eCsrNextCheckAllowConc,
};

enum csr_roamcomplete_result {
	eCsrJoinSuccess,
	eCsrJoinFailure,
	eCsrReassocSuccess,
	eCsrReassocFailure,
	eCsrNothingToJoin,
	eCsrStartBssSuccess,
	eCsrStartBssFailure,
	eCsrSilentlyStopRoaming,
	eCsrSilentlyStopRoamingSaveState,
	eCsrJoinFailureDueToConcurrency,
	eCsrStopBssSuccess,
	eCsrStopBssFailure,
};

struct tag_csrscan_result {
	tListElem Link;
	/* This BSS is removed when it reaches 0 or less */
	int32_t AgingCount;
	/* The bigger the number, the better the BSS.
	 * This value override capValue
	 */
	uint32_t preferValue;
	/* The biggger the better. This value is in use only if we have
	 * equal preferValue
	 */
	uint32_t capValue;
	/* This member must be the last in the structure because the end of
	 * struct bss_description (inside) is an
	 * array with nonknown size at this time
	 */
	/* Preferred Encryption type that matched with profile. */
	eCsrEncryptionType ucEncryptionType;
	eCsrEncryptionType mcEncryptionType;
	/* Preferred auth type that matched with the profile. */
	enum csr_akm_type authType;
	int  bss_score;
	uint8_t retry_count;

	tCsrScanResultInfo Result;
	/*
	 * WARNING - Do not add any element here
	 * This member Result must be the last in the structure because the end
	 * of struct bss_description (inside) is an array with nonknown size at
	 * this time.
	 */
};

struct scan_result_list {
	tDblLinkList List;
	tListElem *pCurEntry;
};

#define CSR_IS_ENC_TYPE_STATIC(encType) ((eCSR_ENCRYPT_TYPE_NONE == (encType)) \
			|| (eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == (encType)) || \
			(eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == (encType)))

#define CSR_IS_AUTH_TYPE_FILS(auth_type) \
		((eCSR_AUTH_TYPE_FILS_SHA256 == auth_type) || \
		(eCSR_AUTH_TYPE_FILS_SHA384 == auth_type) || \
		(eCSR_AUTH_TYPE_FT_FILS_SHA256 == auth_type) || \
		(eCSR_AUTH_TYPE_FT_FILS_SHA384 == auth_type))
#define CSR_IS_WAIT_FOR_KEY(mac, sessionId) \
		 (CSR_IS_ROAM_JOINED(mac, sessionId) && \
		  CSR_IS_ROAM_SUBSTATE_WAITFORKEY(mac, sessionId))
/* WIFI has a test case for not using HT rates with TKIP as encryption */
/* We may need to add WEP but for now, TKIP only. */

#define CSR_IS_11n_ALLOWED(encType) ((eCSR_ENCRYPT_TYPE_TKIP != (encType)) && \
			(eCSR_ENCRYPT_TYPE_WEP40_STATICKEY != (encType)) && \
			(eCSR_ENCRYPT_TYPE_WEP104_STATICKEY != (encType)) && \
				     (eCSR_ENCRYPT_TYPE_WEP40 != (encType)) && \
				       (eCSR_ENCRYPT_TYPE_WEP104 != (encType)))

#define CSR_IS_DISCONNECT_COMMAND(pCommand) ((eSmeCommandRoam == \
		(pCommand)->command) && \
		((eCsrForcedDisassoc == (pCommand)->u.roamCmd.roamReason) || \
		(eCsrForcedDeauth == (pCommand)->u.roamCmd.roamReason) || \
					(eCsrSmeIssuedDisassocForHandoff == \
					(pCommand)->u.roamCmd.roamReason) || \
					(eCsrForcedDisassocMICFailure == \
					(pCommand)->u.roamCmd.roamReason)))

enum csr_roam_state csr_roam_state_change(struct mac_context *mac,
					  enum csr_roam_state NewRoamState,
					  uint8_t sessionId);
void csr_roaming_state_msg_processor(struct mac_context *mac, void *msg_buf);
void csr_roam_joined_state_msg_processor(struct mac_context *mac,
					 void *msg_buf);
void csr_scan_callback(struct wlan_objmgr_vdev *vdev,
				struct scan_event *event, void *arg);
void csr_release_command_roam(struct mac_context *mac, tSmeCmd *pCommand);
void csr_release_command_wm_status_change(struct mac_context *mac,
					  tSmeCmd *pCommand);

QDF_STATUS csr_roam_save_connected_bss_desc(struct mac_context *mac,
					    uint32_t sessionId,
					    struct bss_description *bss_desc);

/*
 * Prepare a filter base on a profile for parsing the scan results.
 * Upon successful return, caller MUST call csr_free_scan_filter on
 * pScanFilter when it is done with the filter.
 */
QDF_STATUS
csr_roam_prepare_filter_from_profile(struct mac_context *mac,
				     struct csr_roam_profile *pProfile,
				     tCsrScanResultFilter *pScanFilter);

QDF_STATUS csr_roam_copy_profile(struct mac_context *mac,
				 struct csr_roam_profile *pDstProfile,
				 struct csr_roam_profile *pSrcProfile);
QDF_STATUS csr_roam_start(struct mac_context *mac);
void csr_roam_stop(struct mac_context *mac, uint32_t sessionId);

QDF_STATUS csr_scan_open(struct mac_context *mac);
QDF_STATUS csr_scan_close(struct mac_context *mac);

bool
csr_scan_append_bss_description(struct mac_context *mac,
				struct bss_description *pSirBssDescription);

QDF_STATUS csr_scan_for_ssid(struct mac_context *mac, uint32_t sessionId,
			     struct csr_roam_profile *pProfile, uint32_t roamId,
			     bool notify);
/**
 * csr_scan_abort_mac_scan() - Generic API to abort scan request
 * @mac: pointer to pmac
 * @vdev_id: pdev id
 * @scan_id: scan id
 *
 * Generic API to abort scans
 *
 * Return: 0 for success, non zero for failure
 */
QDF_STATUS csr_scan_abort_mac_scan(struct mac_context *mac, uint32_t vdev_id,
				   uint32_t scan_id);

/* If fForce is true we will save the new String that is learn't. */
/* Typically it will be true in case of Join or user initiated ioctl */
bool csr_learn_11dcountry_information(struct mac_context *mac,
				   struct bss_description *pSirBssDesc,
				   tDot11fBeaconIEs *pIes, bool fForce);
void csr_apply_country_information(struct mac_context *mac);
void csr_free_scan_result_entry(struct mac_context *mac, struct tag_csrscan_result
				*pResult);

QDF_STATUS csr_roam_call_callback(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_info *roam_info,
				  uint32_t roamId,
				  eRoamCmdStatus u1, eCsrRoamResult u2);
QDF_STATUS csr_roam_issue_connect(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				  tScanResultHandle hBSSList,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate, bool fClearScan);
QDF_STATUS csr_roam_issue_reassoc(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				 tCsrRoamModifyProfileFields *pModProfileFields,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate);
void csr_roam_complete(struct mac_context *mac, enum csr_roamcomplete_result Result,
		       void *Context, uint8_t session_id);
QDF_STATUS
csr_roam_issue_set_context_req_helper(struct mac_context *mac,
				      uint32_t session_id,
				      eCsrEncryptionType encr_type,
				      struct bss_description *bss_descr,
				      tSirMacAddr *bssid, bool addkey,
				      bool unicast,
				      tAniKeyDirection key_direction,
				      uint8_t key_id, uint16_t key_length,
				      uint8_t *key, uint8_t pae_role);

QDF_STATUS csr_roam_process_disassoc_deauth(struct mac_context *mac,
						tSmeCmd *pCommand,
					    bool fDisassoc, bool fMICFailure);

QDF_STATUS
csr_roam_save_connected_information(struct mac_context *mac,
				    uint32_t sessionId,
				    struct csr_roam_profile *pProfile,
				    struct bss_description *pSirBssDesc,
				    tDot11fBeaconIEs *pIes);

void csr_roam_check_for_link_status_change(struct mac_context *mac,
					tSirSmeRsp *pSirMsg);

#ifndef QCA_SUPPORT_CP_STATS
void csr_roam_stats_rsp_processor(struct mac_context *mac, tSirSmeRsp *pSirMsg);
#else
static inline void csr_roam_stats_rsp_processor(struct mac_context *mac,
						tSirSmeRsp *pSirMsg) {}
#endif /* QCA_SUPPORT_CP_STATS */

QDF_STATUS csr_roam_issue_start_bss(struct mac_context *mac, uint32_t sessionId,
				    struct csr_roamstart_bssparams *pParam,
				    struct csr_roam_profile *pProfile,
				    struct bss_description *bss_desc,
					uint32_t roamId);
QDF_STATUS csr_roam_issue_stop_bss(struct mac_context *mac, uint32_t sessionId,
				   enum csr_roam_substate NewSubstate);
bool csr_is_same_profile(struct mac_context *mac, tCsrRoamConnectedProfile
			*pProfile1, struct csr_roam_profile *pProfile2);
bool csr_is_roam_command_waiting_for_session(struct mac_context *mac,
					uint32_t sessionId);
eRoamCmdStatus csr_get_roam_complete_status(struct mac_context *mac,
					    uint32_t sessionId);
/* pBand can be NULL if caller doesn't need to get it */
QDF_STATUS csr_roam_issue_disassociate_cmd(struct mac_context *mac,
					   uint32_t sessionId,
					   eCsrRoamDisconnectReason reason,
					   tSirMacReasonCodes mac_reason);
QDF_STATUS csr_roam_disconnect_internal(struct mac_context *mac, uint32_t sessionId,
					eCsrRoamDisconnectReason reason,
					tSirMacReasonCodes mac_reason);
/* pCommand may be NULL */
void csr_roam_remove_duplicate_command(struct mac_context *mac, uint32_t sessionId,
				       tSmeCmd *pCommand,
				       enum csr_roam_reason eRoamReason);

QDF_STATUS csr_send_join_req_msg(struct mac_context *mac, uint32_t sessionId,
				 struct bss_description *pBssDescription,
				 struct csr_roam_profile *pProfile,
				 tDot11fBeaconIEs *pIes, uint16_t messageType);
QDF_STATUS csr_send_mb_disassoc_req_msg(struct mac_context *mac, uint32_t sessionId,
					tSirMacAddr bssId, uint16_t reasonCode);
QDF_STATUS csr_send_mb_deauth_req_msg(struct mac_context *mac, uint32_t sessionId,
				      tSirMacAddr bssId, uint16_t reasonCode);
QDF_STATUS csr_send_mb_disassoc_cnf_msg(struct mac_context *mac,
					struct disassoc_ind *pDisassocInd);
QDF_STATUS csr_send_mb_deauth_cnf_msg(struct mac_context *mac,
				      struct deauth_ind *pDeauthInd);
QDF_STATUS csr_send_assoc_cnf_msg(struct mac_context *mac,
				  struct assoc_ind *pAssocInd,
				  QDF_STATUS status,
				  enum mac_status_code mac_status_code);
QDF_STATUS csr_send_mb_start_bss_req_msg(struct mac_context *mac,
					 uint32_t sessionId,
					 eCsrRoamBssType bssType,
					 struct csr_roamstart_bssparams *pParam,
					 struct bss_description *bss_desc);
QDF_STATUS csr_send_mb_stop_bss_req_msg(struct mac_context *mac,
					uint32_t sessionId);

/* Caller should put the BSS' ssid to fiedl bssSsid when
 * comparing SSID for a BSS.
 */
bool csr_is_ssid_match(struct mac_context *mac, uint8_t *ssid1, uint8_t ssid1Len,
		       uint8_t *bssSsid, uint8_t bssSsidLen,
			bool fSsidRequired);
bool csr_is_phy_mode_match(struct mac_context *mac, uint32_t phyMode,
			   struct bss_description *pSirBssDesc,
			   struct csr_roam_profile *pProfile,
			   enum csr_cfgdot11mode *pReturnCfgDot11Mode,
			   tDot11fBeaconIEs *pIes);
bool csr_roam_is_channel_valid(struct mac_context *mac, uint8_t channel);

/* pNumChan is a caller allocated space with the sizeof pChannels */
QDF_STATUS csr_get_cfg_valid_channels(struct mac_context *mac, uint8_t *pChannels,
				      uint32_t *pNumChan);
int8_t csr_get_cfg_max_tx_power(struct mac_context *mac, uint8_t channel);

/* To free the last roaming profile */
void csr_free_roam_profile(struct mac_context *mac, uint32_t sessionId);
void csr_free_connect_bss_desc(struct mac_context *mac, uint32_t sessionId);

/* to free memory allocated inside the profile structure */
void csr_release_profile(struct mac_context *mac,
			 struct csr_roam_profile *pProfile);

/* To free memory allocated inside scanFilter */
void csr_free_scan_filter(struct mac_context *mac, tCsrScanResultFilter
			*pScanFilter);

enum csr_cfgdot11mode
csr_get_cfg_dot11_mode_from_csr_phy_mode(struct csr_roam_profile *pProfile,
					 eCsrPhyMode phyMode,
					 bool fProprietary);

uint32_t csr_translate_to_wni_cfg_dot11_mode(struct mac_context *mac,
				    enum csr_cfgdot11mode csrDot11Mode);
void csr_save_channel_power_for_band(struct mac_context *mac, bool fPopulate5GBand);
void csr_apply_channel_power_info_to_fw(struct mac_context *mac,
					struct csr_channel *pChannelList,
					uint8_t *countryCode);
void csr_apply_power2_current(struct mac_context *mac);
void csr_assign_rssi_for_category(struct mac_context *mac, int8_t bestApRssi,
				  uint8_t catOffset);

/* return a bool to indicate whether roaming completed or continue. */
bool csr_roam_complete_roaming(struct mac_context *mac, uint32_t sessionId,
			       bool fForce, eCsrRoamResult roamResult);
void csr_roam_completion(struct mac_context *mac, uint32_t sessionId,
			 struct csr_roam_info *roam_info, tSmeCmd *pCommand,
			 eCsrRoamResult roamResult, bool fSuccess);
void csr_roam_cancel_roaming(struct mac_context *mac, uint32_t sessionId);
void csr_apply_channel_power_info_wrapper(struct mac_context *mac);
void csr_reset_pmkid_candidate_list(struct mac_context *mac, uint32_t sessionId);
QDF_STATUS csr_save_to_channel_power2_g_5_g(struct mac_context *mac,
					uint32_t tableSize, tSirMacChanInfo
					*channelTable);
QDF_STATUS csr_roam_set_key(struct mac_context *mac, uint32_t sessionId,
			    tCsrRoamSetKey *pSetKey, uint32_t roamId);
QDF_STATUS csr_roam_open_session(struct mac_context *mac,
				 struct sme_session_params *session_param);
QDF_STATUS csr_roam_close_session(struct mac_context *mac_ctx,
				  uint32_t session_id, bool sync);
void csr_cleanup_session(struct mac_context *mac, uint32_t sessionId);
QDF_STATUS csr_roam_get_session_id_from_bssid(struct mac_context *mac,
						struct qdf_mac_addr *bssid,
					      uint32_t *pSessionId);
enum csr_cfgdot11mode csr_find_best_phy_mode(struct mac_context *mac,
							uint32_t phyMode);

/*
 * csr_scan_get_result() -
 * Return scan results.
 *
 * pFilter - If pFilter is NULL, all cached results are returned
 * phResult - an object for the result.
 * Return QDF_STATUS
 */
QDF_STATUS csr_scan_get_result(struct mac_context *mac, tCsrScanResultFilter
				*pFilter, tScanResultHandle *phResult);

/**
 * csr_scan_get_result_for_bssid - gets the scan result from scan cache for the
 *      bssid specified
 * @mac_ctx: mac context
 * @bssid: bssid to get the scan result for
 * @res: pointer to tCsrScanResultInfo
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_scan_get_result_for_bssid(struct mac_context *mac_ctx,
					 struct qdf_mac_addr *bssid,
					 tCsrScanResultInfo *res);

/*
 * csr_scan_filter_results() -
 *  Filter scan results based on valid channel list.
 *
 * mac - Pointer to Global MAC structure
 * Return QDF_STATUS
 */
QDF_STATUS csr_scan_filter_results(struct mac_context *mac);

/*
 * csr_scan_result_get_first
 * Returns the first element of scan result.
 *
 * hScanResult - returned from csr_scan_get_result
 * tCsrScanResultInfo * - NULL if no result
 */
tCsrScanResultInfo *csr_scan_result_get_first(struct mac_context *mac,
					      tScanResultHandle hScanResult);
/*
 * csr_scan_result_get_next
 * Returns the next element of scan result. It can be called without calling
 * csr_scan_result_get_first first
 *
 * hScanResult - returned from csr_scan_get_result
 * Return Null if no result or reach the end
 */
tCsrScanResultInfo *csr_scan_result_get_next(struct mac_context *mac,
					     tScanResultHandle hScanResult);

/*
 * csr_get_country_code() -
 * This function is to get the country code current being used
 * pBuf - Caller allocated buffer with at least 3 bytes, upon success return,
 * this has the country code
 * pbLen - Caller allocated, as input, it indicates the length of pBuf. Upon
 * success return, this contains the length of the data in pBuf
 * Return QDF_STATUS
 */
QDF_STATUS csr_get_country_code(struct mac_context *mac, uint8_t *pBuf,
				uint8_t *pbLen);

/*
 * csr_get_regulatory_domain_for_country() -
 * This function is to get the regulatory domain for a country.
 * This function must be called after CFG is downloaded and all the band/mode
 * setting already passed into CSR.
 *
 * pCountry - Caller allocated buffer with at least 3 bytes specifying the
 * country code
 * pDomainId - Caller allocated buffer to get the return domain ID upon
 * success return. Can be NULL.
 * source - the source of country information.
 * Return QDF_STATUS
 */
QDF_STATUS csr_get_regulatory_domain_for_country(struct mac_context *mac,
						 uint8_t *pCountry,
						 v_REGDOMAIN_t *pDomainId,
						 enum country_src source);

/* some support functions */
bool csr_is11h_supported(struct mac_context *mac);
bool csr_is11e_supported(struct mac_context *mac);
bool csr_is_wmm_supported(struct mac_context *mac);

/* Return SUCCESS is the command is queued, failed */
QDF_STATUS csr_queue_sme_command(struct mac_context *mac, tSmeCmd *pCommand,
				 bool fHighPriority);
tSmeCmd *csr_get_command_buffer(struct mac_context *mac);
void csr_release_command(struct mac_context *mac, tSmeCmd *pCommand);
void csr_release_command_buffer(struct mac_context *mac, tSmeCmd *pCommand);

#ifdef FEATURE_WLAN_WAPI
bool csr_is_profile_wapi(struct csr_roam_profile *pProfile);
#endif /* FEATURE_WLAN_WAPI */

void csr_get_vdev_type_nss(struct mac_context *mac_ctx,
		enum QDF_OPMODE dev_mode,
		uint8_t *nss_2g, uint8_t *nss_5g);

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR

/* Security */
#define WLAN_SECURITY_EVENT_REMOVE_KEY_REQ  5
#define WLAN_SECURITY_EVENT_REMOVE_KEY_RSP  6
#define WLAN_SECURITY_EVENT_PMKID_CANDIDATE_FOUND  7
#define WLAN_SECURITY_EVENT_PMKID_UPDATE    8
#define WLAN_SECURITY_EVENT_MIC_ERROR       9
#define WLAN_SECURITY_EVENT_SET_UNICAST_REQ  10
#define WLAN_SECURITY_EVENT_SET_UNICAST_RSP  11
#define WLAN_SECURITY_EVENT_SET_BCAST_REQ    12
#define WLAN_SECURITY_EVENT_SET_BCAST_RSP    13

#define NO_MATCH    0
#define MATCH       1

#define WLAN_SECURITY_STATUS_SUCCESS        0
#define WLAN_SECURITY_STATUS_FAILURE        1

/* Scan */
#define WLAN_SCAN_EVENT_ACTIVE_SCAN_REQ     1
#define WLAN_SCAN_EVENT_ACTIVE_SCAN_RSP     2
#define WLAN_SCAN_EVENT_PASSIVE_SCAN_REQ    3
#define WLAN_SCAN_EVENT_PASSIVE_SCAN_RSP    4
#define WLAN_SCAN_EVENT_HO_SCAN_REQ         5
#define WLAN_SCAN_EVENT_HO_SCAN_RSP         6

#define WLAN_SCAN_STATUS_SUCCESS        0
#define WLAN_SCAN_STATUS_FAILURE        1
#define WLAN_SCAN_STATUS_ABORT          2

/* Ibss */
#define WLAN_IBSS_EVENT_START_IBSS_REQ      0
#define WLAN_IBSS_EVENT_START_IBSS_RSP      1
#define WLAN_IBSS_EVENT_JOIN_IBSS_REQ       2
#define WLAN_IBSS_EVENT_JOIN_IBSS_RSP       3
#define WLAN_IBSS_EVENT_COALESCING          4
#define WLAN_IBSS_EVENT_PEER_JOIN           5
#define WLAN_IBSS_EVENT_PEER_LEAVE          6
#define WLAN_IBSS_EVENT_STOP_REQ            7
#define WLAN_IBSS_EVENT_STOP_RSP            8

#define AUTO_PICK       0
#define SPECIFIED       1

#define WLAN_IBSS_STATUS_SUCCESS        0
#define WLAN_IBSS_STATUS_FAILURE        1

/* 11d */
#define WLAN_80211D_EVENT_COUNTRY_SET   0
#define WLAN_80211D_EVENT_RESET         1

#define WLAN_80211D_DISABLED         0
#define WLAN_80211D_SUPPORT_MULTI_DOMAIN     1
#define WLAN_80211D_NOT_SUPPORT_MULTI_DOMAIN     2

/**
 * diag_auth_type_from_csr_type() - to convert CSR auth type to DIAG auth type
 * @authtype: CSR auth type
 *
 * DIAG tool understands its own ENUMs, so this API can be used to convert
 * CSR defined auth type ENUMs to DIAG defined auth type ENUMs
 *
 *
 * Return: DIAG auth type
 */
enum mgmt_auth_type diag_auth_type_from_csr_type(enum csr_akm_type authtype);
/**
 * diag_enc_type_from_csr_type() - to convert CSR encr type to DIAG encr type
 * @enctype: CSR encryption type
 *
 * DIAG tool understands its own ENUMs, so this API can be used to convert
 * CSR defined encr type ENUMs to DIAG defined encr type ENUMs
 *
 * Return: DIAG encryption type
 */
enum mgmt_encrypt_type diag_enc_type_from_csr_type(eCsrEncryptionType enctype);
/**
 * diag_dot11_mode_from_csr_type() - to convert CSR .11 mode to DIAG .11 mode
 * @dot11mode: CSR 80211 mode
 *
 * DIAG tool understands its own ENUMs, so this API can be used to convert
 * CSR defined 80211 mode ENUMs to DIAG defined 80211 mode ENUMs
 *
 * Return: DIAG 80211mode
 */
enum mgmt_dot11_mode
diag_dot11_mode_from_csr_type(enum csr_cfgdot11mode dot11mode);
/**
 * diag_ch_width_from_csr_type() - to convert CSR ch width to DIAG ch width
 * @ch_width: CSR channel width
 *
 * DIAG tool understands its own ENUMs, so this API can be used to convert
 * CSR defined ch width ENUMs to DIAG defined ch width ENUMs
 *
 * Return: DIAG channel width
 */
enum mgmt_ch_width diag_ch_width_from_csr_type(enum phy_ch_width ch_width);
/**
 * diag_persona_from_csr_type() - to convert QDF persona to DIAG persona
 * @persona: QDF persona
 *
 * DIAG tool understands its own ENUMs, so this API can be used to convert
 * QDF defined persona type ENUMs to DIAG defined persona type ENUMs
 *
 * Return: DIAG persona
 */
enum mgmt_bss_type diag_persona_from_csr_type(enum QDF_OPMODE persona);
#endif /* #ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR */
/*
 * csr_scan_result_purge() -
 * Remove all items(tCsrScanResult) in the list and free memory for each item
 * hScanResult - returned from csr_scan_get_result. hScanResult is considered
 * gone by calling this function and even before this function reutrns.
 * Return QDF_STATUS
 */
QDF_STATUS csr_scan_result_purge(struct mac_context *mac,
				 tScanResultHandle hScanResult);

/* /////////////////////////////////////////Common Scan ends */

/*
 * csr_roam_connect() -
 * To inititiate an association
 * pProfile - can be NULL to join to any open ones
 * pRoamId - to get back the request ID
 * Return QDF_STATUS
 */
QDF_STATUS csr_roam_connect(struct mac_context *mac, uint32_t sessionId,
			    struct csr_roam_profile *pProfile,
			    uint32_t *pRoamId);

/*
 * csr_roam_reassoc() -
 * To inititiate a re-association
 * pProfile - can be NULL to join the currently connected AP. In that
 * case modProfileFields should carry the modified field(s) which could trigger
 * reassoc
 * modProfileFields - fields which are part of tCsrRoamConnectedProfile
 * that might need modification dynamically once STA is up & running and this
 * could trigger a reassoc
 * pRoamId - to get back the request ID
 * Return QDF_STATUS
 */
QDF_STATUS csr_roam_reassoc(struct mac_context *mac, uint32_t sessionId,
			    struct csr_roam_profile *pProfile,
			    tCsrRoamModifyProfileFields modProfileFields,
			    uint32_t *pRoamId);

/*
 * csr_roam_set_pmkid_cache() -
 * return the PMKID candidate list
 *
 * pPMKIDCache - caller allocated buffer point to an array of tPmkidCacheInfo
 * numItems - a variable that has the number of tPmkidCacheInfo allocated
 * when retruning, this is either the number needed or number of items put
 * into pPMKIDCache
 * Return QDF_STATUS - when fail, it usually means the buffer allocated is not
 * big enough and pNumItems has the number of tPmkidCacheInfo.
 * \Note: pNumItems is a number of tPmkidCacheInfo, not
 * sizeof(tPmkidCacheInfo) * something
 */
QDF_STATUS csr_roam_set_pmkid_cache(struct mac_context *mac, uint32_t sessionId,
				    tPmkidCacheInfo *pPMKIDCache,
				   uint32_t numItems, bool update_entire_cache);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/*
 * csr_get_pmk_info(): store PMK in pmk_cache
 * @mac_ctx: pointer to global structure for MAC
 * @session_id: Sme session id
 * @pmk_cache: pointer to a structure of Pmk
 *
 * This API gets the PMK from the session and
 * stores it in the pmk_cache
 *
 * Return: none
 */
void csr_get_pmk_info(struct mac_context *mac_ctx, uint8_t session_id,
		      tPmkidCacheInfo *pmk_cache);

/*
 * csr_roam_set_psk_pmk() -
 * store PSK/PMK
 * mac  - pointer to global structure for MAC
 * sessionId - Sme session id
 * pPSK_PMK - pointer to an array of Psk/Pmk
 * Return QDF_STATUS - usually it succeed unless sessionId is not found
 * Note:
 */
QDF_STATUS csr_roam_set_psk_pmk(struct mac_context *mac, uint32_t sessionId,
				uint8_t *pPSK_PMK, size_t pmk_len);

QDF_STATUS csr_roam_set_key_mgmt_offload(struct mac_context *mac_ctx,
					 uint32_t session_id,
					 bool roam_key_mgmt_offload_enabled,
					 struct pmkid_mode_bits *pmkid_modes);
#endif
/*
 * csr_roam_get_wpa_rsn_req_ie() -
 * Return the WPA or RSN IE CSR passes to PE to JOIN request or START_BSS
 * request
 * pLen - caller allocated memory that has the length of pBuf as input.
 * Upon returned, *pLen has the needed or IE length in pBuf.
 * pBuf - Caller allocated memory that contain the IE field, if any, upon return
 * Return QDF_STATUS - when fail, it usually means the buffer allocated is not
 * big enough
 */
QDF_STATUS csr_roam_get_wpa_rsn_req_ie(struct mac_context *mac, uint32_t sessionId,
				       uint32_t *pLen, uint8_t *pBuf);

/*
 * csr_roam_get_wpa_rsn_rsp_ie() -
 * Return the WPA or RSN IE from the beacon or probe rsp if connected
 *
 * pLen - caller allocated memory that has the length of pBuf as input.
 * Upon returned, *pLen has the needed or IE length in pBuf.
 * pBuf - Caller allocated memory that contain the IE field, if any, upon return
 * Return QDF_STATUS - when fail, it usually means the buffer allocated is not
 * big enough
 */
QDF_STATUS csr_roam_get_wpa_rsn_rsp_ie(struct mac_context *mac, uint32_t sessionId,
				       uint32_t *pLen, uint8_t *pBuf);

/**
 * csr_roam_get_connect_profile() - To return the current connect profile,
 * caller must call csr_roam_free_connect_profile after it is done and before
 * reuse for another csr_roam_get_connect_profile call.
 *
 * @mac:          pointer to global adapter context
 * @sessionId:     session ID
 * @pProfile:      pointer to a caller allocated structure
 *                 tCsrRoamConnectedProfile
 *
 * Return: QDF_STATUS. Failure if not connected, success otherwise
 */
QDF_STATUS csr_roam_get_connect_profile(struct mac_context *mac, uint32_t sessionId,
					tCsrRoamConnectedProfile *pProfile);

void csr_roam_free_connect_profile(tCsrRoamConnectedProfile *profile);

/*
 * csr_apply_channel_and_power_list() -
 *  HDD calls this function to set the CFG_VALID_CHANNEL_LIST base on the
 * band/mode settings. This function must be called after CFG is downloaded
 * and all the band/mode setting already passed into CSR.

 * Return QDF_STATUS
 */
QDF_STATUS csr_apply_channel_and_power_list(struct mac_context *mac);

/*
 * csr_roam_disconnect() - To disconnect from a network
 * @mac: pointer to mac context
 * @session_id: Session ID
 * @reason: CSR disconnect reason code as per @enum eCsrRoamDisconnectReason
 * @mac_reason: Mac Disconnect reason code as per @enum eSirMacReasonCodes
 *
 * Return QDF_STATUS
 */
QDF_STATUS csr_roam_disconnect(struct mac_context *mac, uint32_t session_id,
			       eCsrRoamDisconnectReason reason,
			       tSirMacReasonCodes mac_reason);

/* This function is used to stop a BSS. It is similar of csr_roamIssueDisconnect
 * but this function doesn't have any logic other than blindly trying to stop
 * BSS
 */
QDF_STATUS csr_roam_issue_stop_bss_cmd(struct mac_context *mac, uint32_t sessionId,
				       bool fHighPriority);

void csr_call_roaming_completion_callback(struct mac_context *mac,
					  struct csr_roam_session *pSession,
					  struct csr_roam_info *roam_info,
					  uint32_t roamId,
					  eCsrRoamResult roamResult);
/**
 * csr_roam_issue_disassociate_sta_cmd() - disassociate a associated station
 * @mac:          Pointer to global structure for MAC
 * @sessionId:     Session Id for Soft AP
 * @p_del_sta_params: Pointer to parameters of the station to disassoc
 *
 * CSR function that HDD calls to issue a deauthenticate station command
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_* on error
 */
QDF_STATUS csr_roam_issue_disassociate_sta_cmd(struct mac_context *mac,
					       uint32_t sessionId,
					       struct csr_del_sta_params
					       *p_del_sta_params);
/**
 * csr_roam_issue_deauth_sta_cmd() - issue deauthenticate station command
 * @mac:          Pointer to global structure for MAC
 * @sessionId:     Session Id for Soft AP
 * @pDelStaParams: Pointer to parameters of the station to deauthenticate
 *
 * CSR function that HDD calls to issue a deauthenticate station command
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_** on error
 */
QDF_STATUS csr_roam_issue_deauth_sta_cmd(struct mac_context *mac,
		uint32_t sessionId,
		struct csr_del_sta_params *pDelStaParams);

/*
 * csr_send_chng_mcc_beacon_interval() -
 *   csr function that HDD calls to send Update beacon interval
 *
 * sessionId - session Id for Soft AP
 * Return QDF_STATUS
 */
QDF_STATUS
csr_send_chng_mcc_beacon_interval(struct mac_context *mac, uint32_t sessionId);

/**
 * csr_roam_ft_pre_auth_rsp_processor() - Handle the preauth response
 * @mac_ctx: Global MAC context
 * @preauth_rsp: Received preauthentication response
 *
 * Return: None
 */
#ifdef WLAN_FEATURE_HOST_ROAM
void csr_roam_ft_pre_auth_rsp_processor(struct mac_context *mac_ctx,
					tpSirFTPreAuthRsp pFTPreAuthRsp);
#else
static inline
void csr_roam_ft_pre_auth_rsp_processor(struct mac_context *mac_ctx,
					tpSirFTPreAuthRsp pFTPreAuthRsp)
{}
#endif

#if defined(FEATURE_WLAN_ESE)
void update_cckmtsf(uint32_t *timeStamp0, uint32_t *timeStamp1,
		    uint64_t *incr);
#endif

QDF_STATUS csr_roam_enqueue_preauth(struct mac_context *mac, uint32_t sessionId,
				    struct bss_description *pBssDescription,
				    enum csr_roam_reason reason,
				    bool fImmediate);
QDF_STATUS csr_dequeue_roam_command(struct mac_context *mac,
				enum csr_roam_reason reason,
				uint8_t session_id);
void csr_init_occupied_channels_list(struct mac_context *mac, uint8_t sessionId);
bool csr_neighbor_roam_connected_profile_match(struct mac_context *mac,
					       uint8_t sessionId,
					       struct tag_csrscan_result
						*pResult,
					       tDot11fBeaconIEs *pIes);

QDF_STATUS csr_scan_create_entry_in_scan_cache(struct mac_context *mac,
						uint32_t sessionId,
						struct qdf_mac_addr bssid,
						uint8_t channel);

QDF_STATUS csr_update_channel_list(struct mac_context *mac);
QDF_STATUS csr_roam_del_pmkid_from_cache(struct mac_context *mac,
					 uint32_t sessionId,
					 tPmkidCacheInfo *pmksa,
					 bool flush_cache);

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_clear_sae_single_pmk - API to clear single_pmk_info cache
 * @pmac: mac context
 * @vdev_id: session id
 * @pmksa: pmk info
 *
 * Return : None
 */
void csr_clear_sae_single_pmk(struct mac_context *mac,
			      uint8_t vdev_id, tPmkidCacheInfo *pmksa);

#else
static inline void
csr_clear_sae_single_pmk(struct mac_context *mac, uint8_t vdev_id,
			 tPmkidCacheInfo *pmksa)
{
}
#endif

QDF_STATUS csr_send_ext_change_channel(struct mac_context *mac_ctx,
				uint32_t channel, uint8_t session_id);

/**
 * csr_csa_start() - request CSA IE transmission from PE
 * @mac_ctx: handle returned by mac_open
 * @session_id: SAP session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_csa_restart(struct mac_context *mac_ctx, uint8_t session_id);

/**
 * csr_sta_continue_csa() - Continue for CSA for STA after HW mode change
 * @mac_ctx: handle returned by mac_open
 * @vdev_id: STA VDEV ID
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_sta_continue_csa(struct mac_context *mac_ctx,
				uint8_t vdev_id);

#ifdef QCA_HT_2040_COEX
QDF_STATUS csr_set_ht2040_mode(struct mac_context *mac, uint32_t sessionId,
			       ePhyChanBondState cbMode, bool obssEnabled);
#endif
QDF_STATUS csr_scan_handle_search_for_ssid(struct mac_context *mac_ctx,
					   uint32_t session_id);
QDF_STATUS csr_scan_handle_search_for_ssid_failure(struct mac_context *mac,
		uint32_t session_id);
void csr_saved_scan_cmd_free_fields(struct mac_context *mac_ctx,
				    struct csr_roam_session *session);
struct bss_description*
csr_get_fst_bssdescr_ptr(tScanResultHandle result_handle);

struct bss_description*
csr_get_bssdescr_from_scan_handle(tScanResultHandle result_handle,
				  struct bss_description *bss_descr);

bool is_disconnect_pending(struct mac_context *mac_ctx,
				   uint8_t sessionid);

QDF_STATUS
csr_roam_prepare_bss_config_from_profile(struct mac_context *mac_ctx,
					 struct csr_roam_profile *profile,
					 struct bss_config_param *bss_cfg,
					 struct bss_description *bss_desc);

void
csr_roam_prepare_bss_params(struct mac_context *mac_ctx, uint32_t session_id,
			    struct csr_roam_profile *profile,
			    struct bss_description *bss_desc,
			    struct bss_config_param *bss_cfg,
			    tDot11fBeaconIEs *ies);

/**
 * csr_remove_bssid_from_scan_list() - remove the bssid from
 * scan list
 * @mac_tx: mac context.
 * @bssid: bssid to be removed
 *
 * This function remove the given bssid from scan list.
 *
 * Return: void.
 */
void csr_remove_bssid_from_scan_list(struct mac_context *mac_ctx,
				     tSirMacAddr bssid);

QDF_STATUS
csr_roam_set_bss_config_cfg(struct mac_context *mac_ctx, uint32_t session_id,
			    struct csr_roam_profile *profile,
			    struct bss_description *bss_desc,
			    struct bss_config_param *bss_cfg,
			    tDot11fBeaconIEs *ies, bool reset_country);

void csr_prune_channel_list_for_mode(struct mac_context *mac,
				     struct csr_channel *pChannelList);

#ifdef WLAN_FEATURE_11W
bool csr_is_mfpc_capable(struct sDot11fIERSN *rsn);
#else
static inline bool csr_is_mfpc_capable(struct sDot11fIERSN *rsn)
{
	return false;
}
#endif

/**
 * csr_get_rf_band()
 *
 * @channel: channel number
 *
 * This function is used to translate channel number to band
 *
 * Return: BAND_2G -  if 2.4GHZ channel
 *         BAND_5G -  if 5GHZ channel
 */
enum band_info csr_get_rf_band(uint8_t channel);

/**
 * csr_lookup_pmkid_using_bssid() - lookup pmkid using bssid
 * @mac: pointer to mac
 * @session: sme session pointer
 * @pmk_cache: pointer to pmk cache
 * @index: index value needs to be seached
 *
 * Return: true if pmkid is found else false
 */
bool csr_lookup_pmkid_using_bssid(struct mac_context *mac,
					struct csr_roam_session *session,
					tPmkidCacheInfo *pmk_cache,
					uint32_t *index);

/**
 * csr_is_pmkid_found_for_peer() - check if pmkid sent by peer is present
				   in PMK cache. Used in SAP mode.
 * @mac: pointer to mac
 * @session: sme session pointer
 * @peer_mac_addr: mac address of the connecting peer
 * @pmkid: pointer to pmkid(s) send by peer
 * @pmkid_count: number of pmkids sent by peer
 *
 * Return: true if pmkid is found else false
 */
bool csr_is_pmkid_found_for_peer(struct mac_context *mac,
				 struct csr_roam_session *session,
				 tSirMacAddr peer_mac_addr,
				 uint8_t *pmkid, uint16_t pmkid_count);
#ifdef WLAN_FEATURE_11AX
void csr_update_session_he_cap(struct mac_context *mac_ctx,
			struct csr_roam_session *session);
void csr_init_session_twt_cap(struct csr_roam_session *session,
			      uint32_t type_of_persona);
#else
static inline void csr_update_session_he_cap(struct mac_context *mac_ctx,
			struct csr_roam_session *session)
{
}

static inline void csr_init_session_twt_cap(struct csr_roam_session *session,
					    uint32_t type_of_persona)
{
}
#endif
/**
 * csr_get_channel_for_hw_mode_change() - This function to find
 *                                       out if HW mode change
 *                                       is needed for any of
 *                                       the candidate AP which
 *                                       STA could join
 * @mac_ctx: mac context
 * @result_handle: an object for the result.
 * @session_id: STA session ID
 *
 * This function is written to find out for any bss from scan
 * handle a HW mode change to DBS will be needed or not.
 *
 * Return: AP channel for which DBS HW mode will be needed. 0
 * means no HW mode change is needed.
 */
uint8_t
csr_get_channel_for_hw_mode_change(struct mac_context *mac_ctx,
				   tScanResultHandle result_handle,
				   uint32_t session_id);

/**
 * csr_scan_get_channel_for_hw_mode_change() - This function to find
 *                                       out if HW mode change
 *                                       is needed for any of
 *                                       the candidate AP which
 *                                       STA could join
 * @mac_ctx: mac context
 * @session_id: STA session ID
 * @profile: profile
 *
 * This function is written to find out for any bss from scan
 * handle a HW mode change to DBS will be needed or not.
 * If there is no candidate AP which requires DBS, this function will return
 * the first Candidate AP's chan.
 *
 * Return: AP channel for which HW mode change will be needed. 0
 * means no candidate AP to connect.
 */
uint8_t
csr_scan_get_channel_for_hw_mode_change(
	struct mac_context *mac_ctx, uint32_t session_id,
	struct csr_roam_profile *profile);

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
/**
 * csr_get_sta_cxn_info() - This function populates all the connection
 *			    information which is formed by DUT-STA to AP
 * @mac_ctx: pointer to mac context
 * @session: pointer to sta session
 * @conn_profile: pointer to connected DUTSTA-REFAP profile
 * @buf: pointer to char buffer to write all the connection information.
 * @buf_size: maximum size of the provided buffer
 *
 * Returns: None (information gets populated in buffer)
 */
void csr_get_sta_cxn_info(struct mac_context *mac_ctx,
			  struct csr_roam_session *session,
			  struct tagCsrRoamConnectedProfile *conn_profile,
			  char *buf, uint32_t buf_sz);
#endif
#endif /* CSR_INSIDE_API_H__ */

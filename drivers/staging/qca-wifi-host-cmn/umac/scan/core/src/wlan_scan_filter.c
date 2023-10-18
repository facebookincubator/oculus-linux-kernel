/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
/*
 * DOC: contains scan cache filter logic
 */

#include <wlan_scan_utils_api.h>
#include "wlan_scan_main.h"
#include "wlan_scan_cache_db_i.h"
#include <wlan_dfs_utils_api.h>
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#include "wlan_reg_services_api.h"

/**
 * scm_check_open() - Check if scan entry support open authmode
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if open security else false
 */
static bool scm_check_open(struct scan_filter *filter,
			   struct scan_cache_entry *db_entry,
			   struct security_info *security)
{
	if (db_entry->cap_info.wlan_caps.privacy) {
		scm_debug(QDF_MAC_ADDR_FMT" : have privacy set",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (filter->ucastcipherset &&
	   !(QDF_HAS_PARAM(filter->ucastcipherset, WLAN_CRYPTO_CIPHER_NONE))) {
		scm_debug(QDF_MAC_ADDR_FMT" : Filter doesn't have CIPHER none in uc %x",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  filter->ucastcipherset);
		return false;
	}

	if (filter->mcastcipherset &&
	   !(QDF_HAS_PARAM(filter->mcastcipherset, WLAN_CRYPTO_CIPHER_NONE))) {
		scm_debug(QDF_MAC_ADDR_FMT" : Filter doesn't have CIPHER none in mc %x",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  filter->mcastcipherset);
		return false;
	}

	QDF_SET_PARAM(security->ucastcipherset, WLAN_CRYPTO_CIPHER_NONE);
	QDF_SET_PARAM(security->mcastcipherset, WLAN_CRYPTO_CIPHER_NONE);

	return true;
}

/**
 * scm_check_wep() - Check if scan entry support WEP authmode
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if WEP security else false
 */
static bool scm_check_wep(struct scan_filter *filter,
			  struct scan_cache_entry *db_entry,
			  struct security_info *security)
{
	/* If privacy bit is not set, consider no match */
	if (!db_entry->cap_info.wlan_caps.privacy) {
		scm_debug(QDF_MAC_ADDR_FMT" : doesn't have privacy set",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (!(db_entry->security_type & SCAN_SECURITY_TYPE_WEP)) {
		scm_debug(QDF_MAC_ADDR_FMT" : doesn't support WEP",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (!filter->ucastcipherset || !filter->mcastcipherset) {
		scm_debug(QDF_MAC_ADDR_FMT" : Filter uc %x or mc %x cipher are 0",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  filter->ucastcipherset,
			  filter->mcastcipherset);
		return false;
	}

	if (!(QDF_HAS_PARAM(filter->ucastcipherset, WLAN_CRYPTO_CIPHER_WEP) ||
	     QDF_HAS_PARAM(filter->ucastcipherset, WLAN_CRYPTO_CIPHER_WEP_40) ||
	     QDF_HAS_PARAM(filter->ucastcipherset,
			   WLAN_CRYPTO_CIPHER_WEP_104))) {
		scm_debug(QDF_MAC_ADDR_FMT" : Filter doesn't have WEP cipher in uc %x",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  filter->ucastcipherset);
		return false;
	}

	if (!(QDF_HAS_PARAM(filter->mcastcipherset, WLAN_CRYPTO_CIPHER_WEP) ||
	     QDF_HAS_PARAM(filter->mcastcipherset, WLAN_CRYPTO_CIPHER_WEP_40) ||
	     QDF_HAS_PARAM(filter->mcastcipherset,
			   WLAN_CRYPTO_CIPHER_WEP_104))) {
		scm_debug(QDF_MAC_ADDR_FMT" : Filter doesn't have WEP cipher in mc %x",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  filter->mcastcipherset);
		return false;
	}

	if (QDF_HAS_PARAM(filter->ucastcipherset, WLAN_CRYPTO_CIPHER_WEP))
		QDF_SET_PARAM(security->ucastcipherset, WLAN_CRYPTO_CIPHER_WEP);

	if (QDF_HAS_PARAM(filter->ucastcipherset, WLAN_CRYPTO_CIPHER_WEP_40))
		QDF_SET_PARAM(security->ucastcipherset,
			      WLAN_CRYPTO_CIPHER_WEP_40);

	if (QDF_HAS_PARAM(filter->ucastcipherset, WLAN_CRYPTO_CIPHER_WEP_104))
		QDF_SET_PARAM(security->ucastcipherset,
			      WLAN_CRYPTO_CIPHER_WEP_104);

	if (QDF_HAS_PARAM(filter->mcastcipherset, WLAN_CRYPTO_CIPHER_WEP))
		QDF_SET_PARAM(security->mcastcipherset, WLAN_CRYPTO_CIPHER_WEP);

	if (QDF_HAS_PARAM(filter->mcastcipherset, WLAN_CRYPTO_CIPHER_WEP_40))
		QDF_SET_PARAM(security->mcastcipherset,
			      WLAN_CRYPTO_CIPHER_WEP_40);

	if (QDF_HAS_PARAM(filter->mcastcipherset, WLAN_CRYPTO_CIPHER_WEP_104))
		QDF_SET_PARAM(security->mcastcipherset,
			      WLAN_CRYPTO_CIPHER_WEP_104);

	return true;
}

/**
 * scm_chk_if_cipher_n_akm_match() - Check if akm and ciphers match
 * @filter: scan filter
 * @ap_crypto: aps crypto params
 *
 * Return: true if matches
 */
static bool scm_chk_if_cipher_n_akm_match(struct scan_filter *filter,
					  struct wlan_crypto_params *ap_crypto)
{
	/* Check AP's pairwise ciphers.*/
	if (!(filter->ucastcipherset & ap_crypto->ucastcipherset))
		return false;

	/* Check AP's group cipher match.*/
	if (!(filter->mcastcipherset & ap_crypto->mcastcipherset))
		return false;

	/* Check AP's AKM match with filter's AKM.*/
	if (!(filter->key_mgmt & ap_crypto->key_mgmt))
		return false;

	/* Check AP's mgmt cipher match if present.*/
	if ((filter->mgmtcipherset && ap_crypto->mgmtcipherset) &&
	    !(filter->mgmtcipherset & ap_crypto->mgmtcipherset))
		return false;

	if (filter->ignore_pmf_cap)
		return true;

	if (filter->pmf_cap == WLAN_PMF_REQUIRED &&
	    !(ap_crypto->rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED))
		return false;

	if (filter->pmf_cap == WLAN_PMF_DISABLED &&
	    (ap_crypto->rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED))
		return false;

	return true;
}

static bool scm_chk_crypto_params(struct scan_filter *filter,
				  struct wlan_crypto_params *ap_crypto,
				  bool is_adaptive_11r,
				  struct scan_cache_entry *db_entry,
				  struct security_info *security)
{
	if (!scm_chk_if_cipher_n_akm_match(filter, ap_crypto)) {
		scm_debug(QDF_MAC_ADDR_FMT": fail. adaptive 11r %d Self: AKM %x CIPHER: mc %x uc %x mgmt %x pmf %d AP: AKM %x CIPHER: mc %x uc %x mgmt %x, RSN caps %x",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes), is_adaptive_11r,
			  filter->key_mgmt, filter->mcastcipherset,
			  filter->ucastcipherset, filter->mgmtcipherset,
			  filter->pmf_cap, ap_crypto->key_mgmt,
			  ap_crypto->mcastcipherset, ap_crypto->ucastcipherset,
			  ap_crypto->mgmtcipherset, ap_crypto->rsn_caps);
		return false;
	}

	security->mcastcipherset =
		ap_crypto->mcastcipherset & filter->mcastcipherset;
	security->ucastcipherset =
		ap_crypto->ucastcipherset & filter->ucastcipherset;
	security->key_mgmt = ap_crypto->key_mgmt & filter->key_mgmt;
	security->rsn_caps = ap_crypto->rsn_caps;

	return true;
}

#ifdef WLAN_ADAPTIVE_11R
/**
 * scm_check_and_update_adaptive_11r_key_mgmt_support() -  check and update
 * first rsn security which is present in RSN IE of Beacon/Probe response to
 * corresponding FT AKM.
 * @ap_crypto: crypto param structure
 *
 * Return: none
 */
static void scm_check_and_update_adaptive_11r_key_mgmt_support(
					struct wlan_crypto_params *ap_crypto)
{
	uint32_t i, first_key_mgmt = WLAN_CRYPTO_KEY_MGMT_MAX;

	/*
	 * Supplicant compares AKM(s) in RSN IE of Beacon/Probe response and
	 * AKM on EAPOL M3 frame received by AP.  In the case of multi AKM,
	 * previously Host converts all adaptive 11r AKM(s), if any, present
	 * in RSN IE of Beacon/Probe response to corresponding FT AKM but the
	 * AP(s) which support adaptive 11r (ADAPTIVE_11R_OUI: 0x964000) only
	 * converts first AKM to corresponding FT AKM and sends EAPOL M3 frame
	 * to DUT. This results in failure in a 4-way handshake in supplicant
	 * due to RSN IE miss-match between RSNIE sent by host and RSNIE
	 * present in EAPOL M3 frame. Now like AP, the host is converting only
	 * the first AKM to corresponding FT AKM to avoid RSNIE mismatch in
	 * supplicant.
	 */
	for (i = 0; i < WLAN_CRYPTO_KEY_MGMT_MAX; i++) {
		if (ap_crypto->akm_list[i].key_mgmt ==
		    WLAN_CRYPTO_KEY_MGMT_IEEE8021X ||
		    ap_crypto->akm_list[i].key_mgmt ==
		    WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256) {
			first_key_mgmt = WLAN_CRYPTO_KEY_MGMT_IEEE8021X;
			break;
		}

		if (ap_crypto->akm_list[i].key_mgmt ==
		    WLAN_CRYPTO_KEY_MGMT_PSK ||
		    ap_crypto->akm_list[i].key_mgmt ==
		    WLAN_CRYPTO_KEY_MGMT_PSK_SHA256) {
			first_key_mgmt = WLAN_CRYPTO_KEY_MGMT_PSK;
			break;
		}
	}

	if (first_key_mgmt == WLAN_CRYPTO_KEY_MGMT_MAX) {
		scm_debug("No adaptive 11r's AKM present in RSN IE");
		return;
	}

	scm_debug("First AKM:%d present in RSN IE of bcn/Probe rsp at index:%d",
		  first_key_mgmt, i);

	if (first_key_mgmt == WLAN_CRYPTO_KEY_MGMT_IEEE8021X)
		QDF_SET_PARAM(ap_crypto->key_mgmt,
			      WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X);

	if (first_key_mgmt == WLAN_CRYPTO_KEY_MGMT_PSK)
		QDF_SET_PARAM(ap_crypto->key_mgmt, WLAN_CRYPTO_KEY_MGMT_FT_PSK);
}
#else
static inline void scm_check_and_update_adaptive_11r_key_mgmt_support(
					struct wlan_crypto_params *ap_crypto)
{
}
#endif

/**
 * scm_check_rsn() - Check if scan entry support RSN security
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if RSN security else false
 */
static bool scm_check_rsn(struct scan_filter *filter,
			  struct scan_cache_entry *db_entry,
			  struct security_info *security)
{
	bool is_adaptive_11r;
	QDF_STATUS status;
	struct wlan_crypto_params *ap_crypto;
	bool match;

	if (!util_scan_entry_rsn(db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT" : doesn't have RSN IE",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	ap_crypto = qdf_mem_malloc(sizeof(*ap_crypto));
	if (!ap_crypto)
		return false;
	status = wlan_crypto_rsnie_check(ap_crypto,
					 util_scan_entry_rsn(db_entry));
	if (QDF_IS_STATUS_ERROR(status)) {
		scm_err(QDF_MAC_ADDR_FMT": failed to parse RSN IE, status %d",
			QDF_MAC_ADDR_REF(db_entry->bssid.bytes), status);
		qdf_mem_free(ap_crypto);
		return false;
	}

	is_adaptive_11r = db_entry->adaptive_11r_ap &&
				filter->enable_adaptive_11r;

	/* If adaptive 11r is enabled set the FT AKM for AP */
	if (is_adaptive_11r)
		scm_check_and_update_adaptive_11r_key_mgmt_support(ap_crypto);

	scm_debug("ap_crypto->key_mgmt:%d, filter->key_mgmt:%d",
		  ap_crypto->key_mgmt, filter->key_mgmt);
	match = scm_chk_crypto_params(filter, ap_crypto, is_adaptive_11r,
				      db_entry, security);
	qdf_mem_free(ap_crypto);

	return match;
}

/**
 * scm_check_wpa() - Check if scan entry support WPA security
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if WPA security else false
 */
static bool scm_check_wpa(struct scan_filter *filter,
			  struct scan_cache_entry *db_entry,
			  struct security_info *security)
{
	QDF_STATUS status;
	struct wlan_crypto_params *ap_crypto;
	bool match;

	if (!util_scan_entry_wpa(db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT" : doesn't have WPA IE",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	ap_crypto = qdf_mem_malloc(sizeof(*ap_crypto));
	if (!ap_crypto)
		return false;

	status = wlan_crypto_wpaie_check(ap_crypto,
					 util_scan_entry_wpa(db_entry));
	if (QDF_IS_STATUS_ERROR(status)) {
		scm_err(QDF_MAC_ADDR_FMT": failed to parse WPA IE, status %d",
			QDF_MAC_ADDR_REF(db_entry->bssid.bytes), status);
		qdf_mem_free(ap_crypto);
		return false;
	}

	match = scm_chk_crypto_params(filter, ap_crypto, false,
				      db_entry, security);
	qdf_mem_free(ap_crypto);

	return match;
}

/**
 * scm_check_wapi() - Check if scan entry support WAPI security
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if WAPI security else false
 */
static bool scm_check_wapi(struct scan_filter *filter,
			   struct scan_cache_entry *db_entry,
			   struct security_info *security)
{
	QDF_STATUS status;
	struct wlan_crypto_params *ap_crypto;

	if (!util_scan_entry_wapi(db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT" : doesn't have WAPI IE",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	ap_crypto = qdf_mem_malloc(sizeof(*ap_crypto));
	if (!ap_crypto)
		return false;

	status = wlan_crypto_wapiie_check(ap_crypto,
					  util_scan_entry_wapi(db_entry));
	if (QDF_IS_STATUS_ERROR(status)) {
		scm_err(QDF_MAC_ADDR_FMT": failed to parse WAPI IE, status %d",
			QDF_MAC_ADDR_REF(db_entry->bssid.bytes), status);
		qdf_mem_free(ap_crypto);
		return false;
	}

	if (!scm_chk_if_cipher_n_akm_match(filter, ap_crypto)) {
		scm_debug(QDF_MAC_ADDR_FMT": fail. Self: AKM %x CIPHER: mc %x uc %x mgmt %x pmf %d AP: AKM %x CIPHER: mc %x uc %x mgmt %x, RSN caps %x",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes), filter->key_mgmt,
			  filter->mcastcipherset, filter->ucastcipherset,
			  filter->mgmtcipherset, filter->pmf_cap,
			  ap_crypto->key_mgmt, ap_crypto->mcastcipherset,
			  ap_crypto->ucastcipherset, ap_crypto->mgmtcipherset,
			  ap_crypto->rsn_caps);
		qdf_mem_free(ap_crypto);

		return false;
	}

	security->mcastcipherset =
		ap_crypto->mcastcipherset & filter->mcastcipherset;
	security->ucastcipherset =
		ap_crypto->ucastcipherset & filter->ucastcipherset;
	security->key_mgmt = ap_crypto->key_mgmt & filter->key_mgmt;
	security->rsn_caps = ap_crypto->rsn_caps;
	qdf_mem_free(ap_crypto);

	return true;
}

/**
 * scm_match_any_security() - Check if any security in filter match
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if any security else false
 */
static bool scm_match_any_security(struct scan_filter *filter,
				   struct scan_cache_entry *db_entry,
				   struct security_info *security)
{
	struct wlan_crypto_params *ap_crypto = {0};
	QDF_STATUS status;
	bool match = false;

	ap_crypto = qdf_mem_malloc(sizeof(*ap_crypto));
	if (!ap_crypto)
		return match;

	if (util_scan_entry_rsn(db_entry)) {
		status = wlan_crypto_rsnie_check(ap_crypto,
						 util_scan_entry_rsn(db_entry));
		if (QDF_IS_STATUS_ERROR(status)) {
			scm_err(QDF_MAC_ADDR_FMT": failed to parse RSN IE, status %d",
				QDF_MAC_ADDR_REF(db_entry->bssid.bytes), status);
			goto free;
		}
		security->mcastcipherset = ap_crypto->mcastcipherset;
		security->ucastcipherset = ap_crypto->ucastcipherset;
		security->key_mgmt = ap_crypto->key_mgmt;
		security->rsn_caps = ap_crypto->rsn_caps;
		QDF_SET_PARAM(security->authmodeset, WLAN_CRYPTO_AUTH_RSNA);
		match = true;
		goto free;
	}

	if (util_scan_entry_wpa(db_entry)) {
		status = wlan_crypto_wpaie_check(ap_crypto,
						 util_scan_entry_wpa(db_entry));
		if (QDF_IS_STATUS_ERROR(status)) {
			scm_err(QDF_MAC_ADDR_FMT": failed to parse WPA IE, status %d",
				QDF_MAC_ADDR_REF(db_entry->bssid.bytes), status);
			goto free;
		}
		security->mcastcipherset = ap_crypto->mcastcipherset;
		security->ucastcipherset = ap_crypto->ucastcipherset;
		security->key_mgmt = ap_crypto->key_mgmt;
		security->rsn_caps = ap_crypto->rsn_caps;
		QDF_SET_PARAM(security->authmodeset, WLAN_CRYPTO_AUTH_WPA);
		match = true;
		goto free;
	}

	if (util_scan_entry_wapi(db_entry)) {
		status = wlan_crypto_wapiie_check(ap_crypto,
						util_scan_entry_wapi(db_entry));
		if (QDF_IS_STATUS_ERROR(status)) {
			scm_err(QDF_MAC_ADDR_FMT": failed to parse WPA IE, status %d",
				QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
				status);
			goto free;
		}
		security->mcastcipherset = ap_crypto->mcastcipherset;
		security->ucastcipherset = ap_crypto->ucastcipherset;
		security->key_mgmt = ap_crypto->key_mgmt;
		security->rsn_caps = ap_crypto->rsn_caps;
		QDF_SET_PARAM(security->authmodeset, WLAN_CRYPTO_AUTH_WAPI);
		match = true;
		goto free;
	}

	if (db_entry->cap_info.wlan_caps.privacy) {
		QDF_SET_PARAM(security->ucastcipherset, WLAN_CRYPTO_CIPHER_WEP);
		QDF_SET_PARAM(security->mcastcipherset, WLAN_CRYPTO_CIPHER_WEP);
		QDF_SET_PARAM(security->authmodeset, WLAN_CRYPTO_AUTH_SHARED);
		match = true;
		goto free;
	}

	QDF_SET_PARAM(security->ucastcipherset, WLAN_CRYPTO_CIPHER_NONE);
	QDF_SET_PARAM(security->mcastcipherset, WLAN_CRYPTO_CIPHER_NONE);
	QDF_SET_PARAM(security->authmodeset, WLAN_CRYPTO_AUTH_OPEN);
	match = true;

free:
	qdf_mem_free(ap_crypto);

	return match;
}

/**
 * scm_is_security_match() - Check if security in filter match
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if security match else false
 */
static bool scm_is_security_match(struct scan_filter *filter,
				  struct scan_cache_entry *db_entry,
				  struct security_info *security)
{
	int i;
	bool match = false;

	if (!filter->authmodeset)
		return scm_match_any_security(filter, db_entry, security);

	for (i = 0; i < WLAN_CRYPTO_AUTH_MAX && !match; i++) {
		if (!QDF_HAS_PARAM(filter->authmodeset, i))
			continue;

		security->authmodeset = 0;
		QDF_SET_PARAM(security->authmodeset, i);

		switch (i) {
		case WLAN_CRYPTO_AUTH_NONE:
		case WLAN_CRYPTO_AUTH_OPEN:
		case WLAN_CRYPTO_AUTH_AUTO:
			match = scm_check_open(filter, db_entry, security);
			if (match)
				break;
		/* If not OPEN, then check WEP match */
			fallthrough;
		case WLAN_CRYPTO_AUTH_SHARED:
			match = scm_check_wep(filter, db_entry, security);
			break;
		case WLAN_CRYPTO_AUTH_8021X:
		case WLAN_CRYPTO_AUTH_RSNA:
		case WLAN_CRYPTO_AUTH_CCKM:
		case WLAN_CRYPTO_AUTH_SAE:
		case WLAN_CRYPTO_AUTH_FILS_SK:
			/* First check if there is a RSN match */
			match = scm_check_rsn(filter, db_entry, security);
			break;
		case WLAN_CRYPTO_AUTH_WPA:
			match = scm_check_wpa(filter, db_entry, security);
			break;
		case WLAN_CRYPTO_AUTH_WAPI:/* WAPI */
			match = scm_check_wapi(filter, db_entry, security);
			break;
		default:
			break;
		}
	}

	return match;
}

static bool scm_ignore_ssid_check_for_owe(struct scan_filter *filter,
					  struct scan_cache_entry *db_entry)
{
	bool is_hidden;

	is_hidden = util_scan_entry_is_hidden_ap(db_entry);
	if (is_hidden &&
	    QDF_HAS_PARAM(filter->key_mgmt, WLAN_CRYPTO_KEY_MGMT_OWE) &&
	    util_is_bssid_match(&filter->bssid_hint, &db_entry->bssid))
		return true;

	/* Dump only for hidden SSID as non-hidden are anyway rejected */
	if (is_hidden)
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore hidden AP as key_mgmt 0x%x is not OWE or bssid hint: "
			  QDF_MAC_ADDR_FMT " does not match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  filter->key_mgmt,
			  QDF_MAC_ADDR_REF(filter->bssid_hint.bytes));
	return false;
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * scm_is_fils_config_match() - Check if FILS config matches
 * @filter: scan filter
 * @db_entry: db entry
 *
 * Return: true if FILS config matches else false
 */
static bool scm_is_fils_config_match(struct scan_filter *filter,
				     struct scan_cache_entry *db_entry)
{
	int i;
	struct fils_indication_ie *indication_ie;
	uint8_t *data;
	uint8_t *end_ptr;

	if (!filter->fils_scan_filter.realm_check)
		return true;

	if (!db_entry->ie_list.fils_indication)
		return false;

	indication_ie =
		(struct fils_indication_ie *)db_entry->ie_list.fils_indication;

	/*
	 * Don't validate the realm, if AP advertises realm count as 0
	 * in the FILS indication element
	 */
	if (!indication_ie->realm_identifiers_cnt)
		return true;

	end_ptr = (uint8_t *)indication_ie + indication_ie->len + 2;
	data = indication_ie->variable_data;

	if (indication_ie->is_cache_id_present &&
	    (data + CACHE_IDENTIFIER_LEN) <= end_ptr)
		data += CACHE_IDENTIFIER_LEN;

	if (indication_ie->is_hessid_present &&
	    (data + HESSID_LEN) <= end_ptr)
		data += HESSID_LEN;

	for (i = 1; i <= indication_ie->realm_identifiers_cnt &&
	     (data + REALM_HASH_LEN) <= end_ptr; i++) {
		if (!qdf_mem_cmp(filter->fils_scan_filter.fils_realm,
				 data, REALM_HASH_LEN))
			return true;
		/* Max realm count reached */
		if (indication_ie->realm_identifiers_cnt == i)
			break;

		data = data + REALM_HASH_LEN;
	}

	return false;
}

#else

static inline bool scm_is_fils_config_match(struct scan_filter *filter,
					    struct scan_cache_entry *db_entry)
{
	return true;
}
#endif

static bool scm_check_dot11mode(struct scan_cache_entry *db_entry,
				struct scan_filter *filter)
{
	switch (filter->dot11mode) {
	case ALLOW_ALL:
		break;
	case ALLOW_11N_ONLY:
		if (!util_scan_entry_htcap(db_entry))
			return false;
		break;
	case ALLOW_11AC_ONLY:
		if (!util_scan_entry_vhtcap(db_entry))
			return false;
		break;
	case ALLOW_11AX_ONLY:
		if (!util_scan_entry_hecap(db_entry))
			return false;
		break;
	case ALLOW_11BE_ONLY:
		if (!util_scan_entry_ehtcap(db_entry))
			return false;
		break;
	default:
		scm_debug("Invalid dot11mode filter passed %d",
			  filter->dot11mode);
	}

	return true;
}

#ifdef WLAN_FEATURE_11BE_MLO
static bool util_mlo_filter_match(struct wlan_objmgr_pdev *pdev,
				  struct scan_filter *filter,
				  struct scan_cache_entry *db_entry)
{
	uint8_t i, band_bitmap, assoc_band_bitmap;
	enum reg_wifi_band band;
	struct partner_link_info *partner_link;
	bool is_disabled;

	if (!db_entry->ie_list.multi_link_bv)
		return true;
	if (!filter->band_bitmap)
		return true;

	/* Apply assoc band filter only for assoc link */
	band_bitmap = filter->band_bitmap & 0xf;
	assoc_band_bitmap = (filter->band_bitmap & 0xf0) >> 4;
	band = wlan_reg_freq_to_band(db_entry->channel.chan_freq);
	if ((assoc_band_bitmap && !(band_bitmap & BIT(band) & assoc_band_bitmap)) ||
	    (!assoc_band_bitmap && !(band_bitmap & BIT(band)))) {
		scm_debug("bss freq %d not match band bitmap: 0x%x",
			  db_entry->channel.chan_freq,
			  filter->band_bitmap);
		return false;
	}
	for (i = 0; i < db_entry->ml_info.num_links; i++) {
		partner_link = &db_entry->ml_info.link_info[i];
		band = wlan_reg_freq_to_band(partner_link->freq);

		is_disabled = wlan_reg_is_disable_for_pwrmode(
				    pdev,
				    partner_link->freq,
				    REG_BEST_PWR_MODE);
		if (is_disabled) {
			scm_debug("partner link id %d freq %d disabled : "QDF_MAC_ADDR_FMT,
				  partner_link->link_id,
				  partner_link->freq,
				  QDF_MAC_ADDR_REF(
				  partner_link->link_addr.bytes));
			continue;
		}
		if (band_bitmap & BIT(band)) {
			scm_debug("partner link id %d freq %d match band bitmap: 0x%x "QDF_MAC_ADDR_FMT,
				  partner_link->link_id,
				  partner_link->freq,
				  filter->band_bitmap,
				  QDF_MAC_ADDR_REF(
				  partner_link->link_addr.bytes));
			partner_link->is_valid_link = true;
		}
	}

	return true;
}
#else
static bool util_mlo_filter_match(struct wlan_objmgr_pdev *pdev,
				  struct scan_filter *filter,
				  struct scan_cache_entry *db_entry)
{
	return true;
}
#endif

#ifdef WLAN_FEATURE_11BE
/**
 * util_eht_puncture_valid(): The function finds the puncturing pattern from the
 * IE. If the primary channel is in the punctured list then the channel cannot
 * be used, and this function returns false/invalid.
 * @pdev: pdev device.
 * @db_entry: scan cache entry which will indicate EHT ops.
 *
 * Return - false if the primary channel is punctured and true if otherwise.
 */
static bool util_eht_puncture_valid(struct wlan_objmgr_pdev *pdev,
				    struct scan_cache_entry *db_entry)
{
	struct wlan_ie_ehtops *eht_ops;
	int8_t orig_width;
	enum phy_ch_width width;
	qdf_freq_t center_freq_320m;
	uint16_t orig_puncture_bitmap;
	uint16_t new_puncture_bitmap = 0;
	QDF_STATUS status;
	uint32_t cfreq1;
	uint8_t band_mask = BIT(REG_BAND_6G);

	eht_ops = (struct wlan_ie_ehtops *)util_scan_entry_ehtop(db_entry);
	if (!eht_ops)
		return true;
	if (!QDF_GET_BITS(eht_ops->ehtop_param,
			  EHTOP_INFO_PRESENT_IDX, EHTOP_INFO_PRESENT_BITS))
		return true;
	if (QDF_GET_BITS(eht_ops->ehtop_param,
			 EHTOP_PARAM_DISABLED_SC_BITMAP_PRESENT_IDX,
			 EHTOP_PARAM_DISABLED_SC_BITMAP_PRESENT_BITS)) {
		orig_puncture_bitmap =
			QDF_GET_BITS(eht_ops->disabled_sub_chan_bitmap[0],
				     0, 8);
		orig_puncture_bitmap |=
			QDF_GET_BITS(eht_ops->disabled_sub_chan_bitmap[1],
				     0, 8) << 8;
	} else {
		orig_puncture_bitmap = 0;
	}
	if (!orig_puncture_bitmap)
		return true;

	orig_width = QDF_GET_BITS(eht_ops->control,
				  EHTOP_INFO_CHAN_WIDTH_IDX,
				  EHTOP_INFO_CHAN_WIDTH_BITS);
	/* Check if CCFS bits are present */
	if (QDF_GET_BITS(eht_ops->ehtop_param,
			 EHTOP_INFO_PRESENT_IDX, EHTOP_INFO_PRESENT_BITS))
		cfreq1 = wlan_reg_chan_band_to_freq(pdev, eht_ops->ccfs1,
						    band_mask);
	else
		cfreq1 = 0;

	if (orig_width == WLAN_EHT_CHWIDTH_320) {
		width = CH_WIDTH_320MHZ;
		center_freq_320m = cfreq1;
	} else {
		width = orig_width;
		center_freq_320m = 0;
	}

	/* Find if the primary channel is punctured */
	status = wlan_reg_extract_puncture_by_bw(width,
						 orig_puncture_bitmap,
						 db_entry->channel.chan_freq,
						 center_freq_320m,
						 CH_WIDTH_20MHZ,
						 &new_puncture_bitmap);
	if (QDF_IS_STATUS_ERROR(status) || new_puncture_bitmap) {
		scm_debug(QDF_MAC_ADDR_FMT "freq %d width %d 320m center %d puncture: orig %d new %d status %d",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  db_entry->channel.chan_freq, width, center_freq_320m,
			  orig_puncture_bitmap, new_puncture_bitmap, status);
		return false;
	} else {
		return true;
	}
}
#else
static bool util_eht_puncture_valid(struct wlan_objmgr_pdev *pdev,
				    struct scan_cache_entry *db_entry)
{
	return true;
}
#endif

bool scm_filter_match(struct wlan_objmgr_psoc *psoc,
		      struct scan_cache_entry *db_entry,
		      struct scan_filter *filter,
		      struct security_info *security)
{
	int i;
	bool match = false;
	struct scan_default_params *def_param;
	struct wlan_objmgr_pdev *pdev;

	def_param = wlan_scan_psoc_get_def_params(psoc);
	if (!def_param)
		return false;

	if (db_entry->ssid.length) {
		for (i = 0; i < filter->num_of_ssid; i++) {
			if (util_is_ssid_match(&filter->ssid_list[i],
			   &db_entry->ssid)) {
				match = true;
				break;
			}
		}
	}
	/*
	 * In OWE transition mode, ssid is hidden. And supplicant does not issue
	 * scan with specific ssid prior to connect as in other hidden ssid
	 * cases. Add explicit check to allow OWE when ssid is hidden.
	 */
	if (!match)
		match = scm_ignore_ssid_check_for_owe(filter, db_entry);

	if (!match && filter->num_of_ssid)
		return false;

	match = false;
	for (i = 0; i < filter->num_of_bssid; i++) {
		if (util_is_bssid_match(&filter->bssid_list[i],
		   &db_entry->bssid)) {
			match = true;
			break;
		}
	}

	if (!match && filter->num_of_bssid) {
		/*
		 * Do not print if ssid is not present in filter to avoid
		 * excessive prints
		 */
		if (filter->num_of_ssid)
			scm_debug(QDF_MAC_ADDR_FMT ": Ignore as BSSID not in list (no. of BSSID in list %d)",
				  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
				  filter->num_of_bssid);
		return false;
	}

	if (filter->age_threshold &&
	    filter->age_threshold < util_scan_entry_age(db_entry)) {
		/*
		 * Do not print if bssid/ssid is not present in filter to avoid
		 * excessive prints
		 */
		if (filter->num_of_bssid || filter->num_of_ssid)
			scm_debug(QDF_MAC_ADDR_FMT ": Ignore as age %lu ms is greater than threshold %lu ms",
				  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
				  util_scan_entry_age(db_entry),
				  filter->age_threshold);
		return false;
	}

	if (filter->dot11mode && !scm_check_dot11mode(db_entry, filter)) {
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore as dot11mode %d didn't match phymode %d",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  filter->dot11mode, db_entry->phy_mode);
		return false;
	}

	if (filter->ignore_6ghz_channel &&
	    WLAN_REG_IS_6GHZ_CHAN_FREQ(db_entry->channel.chan_freq)) {
		/*
		 * Do not print if bssid/ssid is not present in filter to avoid
		 * excessive prints
		 */
		if (filter->num_of_bssid || filter->num_of_ssid)
			scm_debug(QDF_MAC_ADDR_FMT ": Ignore as its on 6Ghz freq %d",
				  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
				  db_entry->channel.chan_freq);

		return false;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, db_entry->pdev_id,
					  WLAN_SCAN_ID);
	if (!pdev) {
		scm_err(QDF_MAC_ADDR_FMT ": Ignore as pdev not found",
			QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (filter->ignore_nol_chan &&
	    utils_dfs_is_freq_in_nol(pdev, db_entry->channel.chan_freq)) {
		wlan_objmgr_pdev_release_ref(pdev, WLAN_SCAN_ID);
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore as chan in NOL list",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}
	wlan_objmgr_pdev_release_ref(pdev, WLAN_SCAN_ID);

	match = false;
	for (i = 0; i < filter->num_of_channels; i++) {
		if (!filter->chan_freq_list[i] ||
		    filter->chan_freq_list[i] ==
		    db_entry->channel.chan_freq) {
			match = true;
			break;
		}
	}

	if (!match && filter->num_of_channels) {
		/*
		 * Do not print if bssid/ssid is not present in filter to avoid
		 * excessive prints (e.g RRM case where only freq list is
		 * provided to get AP's in specific frequencies)
		 */
		if (filter->num_of_bssid || filter->num_of_ssid)
			scm_debug(QDF_MAC_ADDR_FMT ": Ignore as AP's freq %d is not in freq list",
				  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
				  db_entry->channel.chan_freq);
		return false;
	}

	if (filter->rrm_measurement_filter)
		return true;

	if (!filter->ignore_auth_enc_type && !filter->match_security_func &&
	    !scm_is_security_match(filter, db_entry, security)) {
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore as security profile didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (filter->match_security_func &&
	    !filter->match_security_func(filter->match_security_func_arg,
					 db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore as custom security match failed",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (filter->ccx_validate_bss &&
	    !filter->ccx_validate_bss(filter->ccx_validate_bss_arg,
				      db_entry, 0)) {
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore as CCX validateion failed",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (!util_is_bss_type_match(filter->bss_type, db_entry->cap_info)) {
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore as bss type didn't match cap_info %x bss_type %d",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  db_entry->cap_info.value, filter->bss_type);
		return false;
	}

	/* Match realm */
	if (!scm_is_fils_config_match(filter, db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT ":Ignore as fils config didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (!util_mdie_match(filter->mobility_domain,
	   (struct rsn_mdie *)db_entry->ie_list.mdie)) {
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore as mdie didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (!util_mlo_filter_match(pdev, filter, db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT ": Ignore as mlo filter didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, db_entry->pdev_id,
					  WLAN_SCAN_ID);
	if (!pdev) {
		scm_err(QDF_MAC_ADDR_FMT ": Ignore as pdev not found",
			QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}
	if (!util_eht_puncture_valid(pdev, db_entry)) {
		wlan_objmgr_pdev_release_ref(pdev, WLAN_SCAN_ID);
		return false;
	}
	wlan_objmgr_pdev_release_ref(pdev, WLAN_SCAN_ID);

	return true;
}

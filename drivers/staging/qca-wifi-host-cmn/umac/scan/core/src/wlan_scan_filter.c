/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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

/**
 * scm_is_open_security() - Check if scan entry support open security
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if open security else false
 */
static bool scm_is_open_security(struct scan_filter *filter,
	struct scan_cache_entry *db_entry,
	struct security_info *security)
{
	bool match = false;
	int i;

	if (db_entry->cap_info.wlan_caps.privacy)
		return false;

	/* Check MC cipher and Auth type requested. */
	for (i = 0; i < filter->num_of_mc_enc_type; i++) {
		if (WLAN_ENCRYPT_TYPE_NONE ==
			filter->mc_enc_type[i]) {
			security->mc_enc =
				filter->mc_enc_type[i];
			match = true;
			break;
		}
	}
	if (!match && filter->num_of_mc_enc_type)
		return match;

	match = false;
	/* Check Auth list. It should contain AuthOpen. */
	for (i = 0; i < filter->num_of_auth; i++) {
		if ((WLAN_AUTH_TYPE_OPEN_SYSTEM ==
			filter->auth_type[i]) ||
			(WLAN_AUTH_TYPE_AUTOSWITCH ==
			filter->auth_type[i])) {
			security->auth_type =
				WLAN_AUTH_TYPE_OPEN_SYSTEM;
			match = true;
			break;
		}
	}

	return match;
}

/**
 * scm_is_cipher_match() - Check if cipher match the cipher list
 * @cipher_list: cipher list to match
 * @num_cipher: number of cipher in cipher list
 * @cipher_to_match: cipher to found in cipher list
 *
 * Return: true if open security else false
 */
static bool scm_is_cipher_match(
	uint32_t *cipher_list,
	uint16_t num_cipher, uint32_t cipher_to_match)
{
	int i;
	bool match = false;

	for (i = 0; i < num_cipher ; i++) {
		match = (cipher_list[i] == cipher_to_match);
		if (match)
			break;
	}

	return match;
}

/**
 * scm_get_cipher_suite_type() - get cypher suite type from enc type
 * @enc: enc type
 *
 * Return: cypher suite type
 */
static uint8_t scm_get_cipher_suite_type(enum wlan_enc_type enc)
{
	uint8_t cipher_type;

	switch (enc) {
	case WLAN_ENCRYPT_TYPE_WEP40:
	case WLAN_ENCRYPT_TYPE_WEP40_STATICKEY:
		cipher_type = WLAN_CSE_WEP40;
		break;
	case WLAN_ENCRYPT_TYPE_WEP104:
	case WLAN_ENCRYPT_TYPE_WEP104_STATICKEY:
		cipher_type = WLAN_CSE_WEP104;
		break;
	case WLAN_ENCRYPT_TYPE_TKIP:
		cipher_type = WLAN_CSE_TKIP;
		break;
	case WLAN_ENCRYPT_TYPE_AES:
		cipher_type = WLAN_CSE_CCMP;
		break;
	case WLAN_ENCRYPT_TYPE_AES_GCMP:
		cipher_type = WLAN_CSE_GCMP_128;
		break;
	case WLAN_ENCRYPT_TYPE_AES_GCMP_256:
		cipher_type = WLAN_CSE_GCMP_256;
		break;
	case WLAN_ENCRYPT_TYPE_NONE:
		cipher_type = WLAN_CSE_NONE;
		break;
	case WLAN_ENCRYPT_TYPE_WPI:
		cipher_type = WLAN_WAI_CERT_OR_SMS4;
		break;
	default:
		cipher_type = WLAN_CSE_RESERVED;
		break;
	}

	return cipher_type;
}

/**
 * scm_is_wep_security() - Check if scan entry support WEP security
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if WEP security else false
 */
static bool scm_is_wep_security(struct scan_filter *filter,
	struct scan_cache_entry *db_entry,
	struct security_info *security)
{
	int i;
	QDF_STATUS status;
	bool match = false;
	enum wlan_auth_type neg_auth = WLAN_AUTH_TYPE_OPEN_SYSTEM;
	enum wlan_enc_type neg_mccipher = WLAN_ENCRYPT_TYPE_NONE;

	if (!security)
		return false;

	/* If privacy bit is not set, consider no match */
	if (!db_entry->cap_info.wlan_caps.privacy)
		return false;

	for (i = 0; i < filter->num_of_mc_enc_type; i++) {
		switch (filter->mc_enc_type[i]) {
		case WLAN_ENCRYPT_TYPE_WEP40_STATICKEY:
		case WLAN_ENCRYPT_TYPE_WEP104_STATICKEY:
		case WLAN_ENCRYPT_TYPE_WEP40:
		case WLAN_ENCRYPT_TYPE_WEP104:
			/*
			 * Multicast list may contain WEP40/WEP104.
			 * Check whether it matches UC.
			 */
			if (security->uc_enc ==
			   filter->mc_enc_type[i]) {
				match = true;
				neg_mccipher =
				   filter->mc_enc_type[i];
			}
			break;
		default:
			match = false;
			break;
		}
		if (match)
			break;
	}

	if (!match)
		return match;

	for (i = 0; i < filter->num_of_auth; i++) {
		switch (filter->auth_type[i]) {
		case WLAN_AUTH_TYPE_OPEN_SYSTEM:
		case WLAN_AUTH_TYPE_SHARED_KEY:
		case WLAN_AUTH_TYPE_AUTOSWITCH:
			match = true;
			neg_auth = filter->auth_type[i];
			break;
		default:
			match = false;
		}
		if (match)
			break;
	}

	if (!match)
		return match;

	/*
	 * In case of WPA / WPA2, check whether it supports WEP as well.
	 * Prepare the encryption type for WPA/WPA2 functions
	 */
	if (security->uc_enc == WLAN_ENCRYPT_TYPE_WEP40_STATICKEY)
		security->uc_enc = WLAN_ENCRYPT_TYPE_WEP40;
	else if (security->uc_enc == WLAN_ENCRYPT_TYPE_WEP104)
		security->uc_enc = WLAN_ENCRYPT_TYPE_WEP104;

	/* else we can use the encryption type directly */
	if (util_scan_entry_wpa(db_entry)) {
		struct wlan_wpa_ie wpa = {0};
		uint8_t cipher_type;

		cipher_type =
			scm_get_cipher_suite_type(security->uc_enc);
		status = wlan_parse_wpa_ie(util_scan_entry_wpa(db_entry), &wpa);
		if (QDF_IS_STATUS_ERROR(status)) {
			scm_err("failed to parse WPA IE, status %d", status);
			scm_hex_dump(QDF_TRACE_LEVEL_DEBUG,
				     util_scan_entry_wpa(db_entry),
				     util_scan_get_wpa_len(db_entry));
			return false;
		}

		match = scm_is_cipher_match(&wpa.mc_cipher,
				  1, WLAN_WPA_SEL(cipher_type));
	}
	if (!match && util_scan_entry_rsn(db_entry)) {
		struct wlan_rsn_ie rsn = {0};
		uint8_t cipher_type;

		cipher_type =
			scm_get_cipher_suite_type(security->uc_enc);
		status = wlan_parse_rsn_ie(util_scan_entry_rsn(db_entry), &rsn);
		if (QDF_IS_STATUS_ERROR(status)) {
			scm_err("failed to parse RSN IE, status %d", status);
			scm_hex_dump(QDF_TRACE_LEVEL_DEBUG,
				     util_scan_entry_rsn(db_entry),
				     util_scan_get_rsn_len(db_entry));
			return false;
		}
		match = scm_is_cipher_match(&rsn.gp_cipher_suite,
				  1, WLAN_RSN_SEL(cipher_type));
	}


	if (match) {
		security->auth_type = neg_auth;
		security->mc_enc = neg_mccipher;
	}

	return match;
}

/**
 * scm_check_pmf_match() - Check PMF security of entry match filter
 * @filter: scan filter
 * @db_entry: ap entry
 * @rsn: rsn IE of the scan entry
 *
 * Return: true if PMF security match else false
 */
static bool
scm_check_pmf_match(struct scan_filter *filter,
		    struct scan_cache_entry *db_entry,
		    struct wlan_rsn_ie *rsn)
{
	enum wlan_pmf_cap ap_pmf_cap = WLAN_PMF_DISABLED;
	bool match = true;

	if (rsn->cap & RSN_CAP_MFP_CAPABLE)
		ap_pmf_cap = WLAN_PMF_CAPABLE;
	if (rsn->cap & RSN_CAP_MFP_REQUIRED)
		ap_pmf_cap = WLAN_PMF_REQUIRED;

	if ((filter->pmf_cap == WLAN_PMF_REQUIRED) &&
		(ap_pmf_cap == WLAN_PMF_DISABLED))
		match = false;
	else if ((filter->pmf_cap == WLAN_PMF_DISABLED) &&
		(ap_pmf_cap == WLAN_PMF_REQUIRED))
		match = false;

	if (!match)
		scm_debug(QDF_MAC_ADDR_FMT" : PMF cap didn't match (filter %d AP %d)",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  filter->pmf_cap,
			  ap_pmf_cap);

	return match;
}

/**
 * scm_is_rsn_mcast_cipher_match() - match the rsn mcast cipher type with AP's
 * mcast cipher
 * @rsn: AP's RSNE
 * @filter: scan filter
 * @neg_mccipher: negotiated mc cipher if matched.
 *
 * Return: true if mc cipher is negotiated
 */
static bool
scm_is_rsn_mcast_cipher_match(struct wlan_rsn_ie *rsn,
	struct scan_filter *filter, enum wlan_enc_type *neg_mccipher)
{
	int i;
	bool match;
	uint8_t cipher_type;

	if (!rsn || !neg_mccipher || !filter)
		return false;

	for (i = 0; i < filter->num_of_mc_enc_type; i++) {

		if (filter->mc_enc_type[i] == WLAN_ENCRYPT_TYPE_ANY) {
			/* Try the more secured ones first. */
			/* Check GCMP_256 first */
			cipher_type = WLAN_CSE_GCMP_256;
			match = scm_is_cipher_match(&rsn->gp_cipher_suite, 1,
						    WLAN_RSN_SEL(cipher_type));
			if (match) {
				*neg_mccipher = WLAN_ENCRYPT_TYPE_AES_GCMP_256;
				return true;
			}
			/* Check GCMP */
			cipher_type = WLAN_CSE_GCMP_128;
			match = scm_is_cipher_match(&rsn->gp_cipher_suite, 1,
						    WLAN_RSN_SEL(cipher_type));
			if (match) {
				*neg_mccipher = WLAN_ENCRYPT_TYPE_AES_GCMP;
				return true;
			}
			/* Check AES */
			cipher_type = WLAN_CSE_CCMP;
			match = scm_is_cipher_match(&rsn->gp_cipher_suite, 1,
						    WLAN_RSN_SEL(cipher_type));
			if (match) {
				*neg_mccipher = WLAN_ENCRYPT_TYPE_AES;
				return true;
			}
			/* Check TKIP */
			cipher_type = WLAN_CSE_TKIP;
			match = scm_is_cipher_match(&rsn->gp_cipher_suite, 1,
						    WLAN_RSN_SEL(cipher_type));
			if (match) {
				*neg_mccipher = WLAN_ENCRYPT_TYPE_TKIP;
				return true;
			}
		} else {
			cipher_type =
			     scm_get_cipher_suite_type(filter->mc_enc_type[i]);
			match = scm_is_cipher_match(&rsn->gp_cipher_suite, 1,
						    WLAN_RSN_SEL(cipher_type));
			if (match) {
				*neg_mccipher = filter->mc_enc_type[i];
				return true;
			}
		}
	}

	return false;
}

/**
 * scm_is_rsn_security() - Check if scan entry support RSN security
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if RSN security else false
 */
static bool scm_is_rsn_security(struct scan_filter *filter,
				struct scan_cache_entry *db_entry,
				struct security_info *security)
{
	int i;
	uint8_t cipher_type;
	bool match_any_akm, match = false;
	enum wlan_auth_type neg_auth = WLAN_NUM_OF_SUPPORT_AUTH_TYPE;
	enum wlan_auth_type filter_akm;
	enum wlan_enc_type neg_mccipher = WLAN_ENCRYPT_TYPE_NONE;
	struct wlan_rsn_ie rsn = {0};
	QDF_STATUS status;
	bool is_adaptive_11r;

	if (!security)
		return false;
	if (!util_scan_entry_rsn(db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT" : doesn't have RSN IE",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}
	status = wlan_parse_rsn_ie(util_scan_entry_rsn(db_entry), &rsn);
	if (QDF_IS_STATUS_ERROR(status)) {
		scm_err("failed to parse RSN IE, status %d", status);
		scm_hex_dump(QDF_TRACE_LEVEL_DEBUG,
			     util_scan_entry_rsn(db_entry),
			     util_scan_get_rsn_len(db_entry));
		return false;
	}

	cipher_type =
		scm_get_cipher_suite_type(security->uc_enc);
	match = scm_is_cipher_match(rsn.pwise_cipher_suites,
		rsn.pwise_cipher_count, WLAN_RSN_SEL(cipher_type));
	if (!match) {
		scm_debug(QDF_MAC_ADDR_FMT" : pairwise cipher didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	match = scm_is_rsn_mcast_cipher_match(&rsn, filter, &neg_mccipher);
	if (!match) {
		scm_debug(QDF_MAC_ADDR_FMT" : mcast cipher didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	is_adaptive_11r = (db_entry->adaptive_11r_ap &&
			   filter->enable_adaptive_11r);

	/* Initializing with false as it has true value already */
	match = false;
	for (i = 0; i < filter->num_of_auth; i++) {

		filter_akm = filter->auth_type[i];
		if (filter_akm == WLAN_AUTH_TYPE_ANY)
			match_any_akm = true;
		else
			match_any_akm = false;
		/*
		 * Ciphers are supported, Match authentication algorithm and
		 * pick first matching authtype.
		 */
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_FILS_FT_SHA384))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_FT_FILS_SHA384)) {
				neg_auth = WLAN_AUTH_TYPE_FT_FILS_SHA384;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_FILS_FT_SHA256))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_FT_FILS_SHA256)) {
				neg_auth = WLAN_AUTH_TYPE_FT_FILS_SHA256;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_FILS_SHA384))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_FILS_SHA384)) {
				neg_auth = WLAN_AUTH_TYPE_FILS_SHA384;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_FILS_SHA256))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_FILS_SHA256)) {
				neg_auth = WLAN_AUTH_TYPE_FILS_SHA256;
				match = true;
				break;
			}
		}

		if (scm_is_cipher_match(rsn.akm_suites,
		    rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_SAE))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_SAE)) {
				neg_auth = WLAN_AUTH_TYPE_SAE;
				match = true;
				break;
			}
		}

		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count, WLAN_RSN_DPP_AKM)) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_DPP_RSN)) {
				neg_auth = WLAN_AUTH_TYPE_DPP_RSN;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(rsn.akm_suites,
					rsn.akm_suite_count,
					WLAN_RSN_OSEN_AKM)) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_OSEN)) {
				neg_auth = WLAN_AUTH_TYPE_OSEN;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_OWE))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_OWE)) {
				neg_auth = WLAN_AUTH_TYPE_OWE;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_FT_IEEE8021X))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_FT_RSN)) {
				neg_auth = WLAN_AUTH_TYPE_FT_RSN;
				match = true;
				break;
			}
		}

		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_FT_PSK))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_FT_RSN_PSK)) {
				neg_auth = WLAN_AUTH_TYPE_FT_RSN_PSK;
				match = true;
				break;
			}
		}
		/* ESE only supports 802.1X.  No PSK. */
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_CCKM_AKM)) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_CCKM_RSN)) {
				neg_auth = WLAN_AUTH_TYPE_CCKM_RSN;
				match = true;
				break;
			}
		}
		/* RSN */
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_IEEE8021X))) {
			if (is_adaptive_11r &&
			    (filter_akm == WLAN_AUTH_TYPE_FT_RSN)) {
				neg_auth = WLAN_AUTH_TYPE_FT_RSN;
				match = true;
				break;
			}

			if (match_any_akm ||
			    (WLAN_AUTH_TYPE_RSN == filter_akm)) {
				neg_auth = WLAN_AUTH_TYPE_RSN;
				match = true;
				break;
			}
		}
		/* TKIP */
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_PSK))) {
			if (is_adaptive_11r &&
			    (filter_akm == WLAN_AUTH_TYPE_FT_RSN_PSK)) {
				neg_auth = WLAN_AUTH_TYPE_FT_RSN_PSK;
				match = true;
				break;
			}

			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_RSN_PSK)) {
				neg_auth = WLAN_AUTH_TYPE_RSN_PSK;
				match = true;
				break;
			}
		}
		/* SHA256 */
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_SHA256_PSK))) {
			if (is_adaptive_11r &&
			    (filter_akm == WLAN_AUTH_TYPE_FT_RSN_PSK)) {
				neg_auth = WLAN_AUTH_TYPE_FT_RSN_PSK;
				match = true;
				break;
			}

			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_RSN_PSK_SHA256)) {
				neg_auth = WLAN_AUTH_TYPE_RSN_PSK_SHA256;
				match = true;
				break;
			}
		}
		/* 8021X SHA256 */
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_SHA256_IEEE8021X))) {
			if (is_adaptive_11r &&
			    (filter_akm == WLAN_AUTH_TYPE_FT_RSN)) {
				neg_auth = WLAN_AUTH_TYPE_FT_RSN;
				match = true;
				break;
			}

			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_RSN_8021X_SHA256)) {
				neg_auth = WLAN_AUTH_TYPE_RSN_8021X_SHA256;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_SUITEB_EAP_SHA256))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_SUITEB_EAP_SHA256)) {
				neg_auth = WLAN_AUTH_TYPE_SUITEB_EAP_SHA256;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(rsn.akm_suites,
		   rsn.akm_suite_count,
		   WLAN_RSN_SEL(WLAN_AKM_SUITEB_EAP_SHA384))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_SUITEB_EAP_SHA384)) {
				neg_auth = WLAN_AUTH_TYPE_SUITEB_EAP_SHA384;
				match = true;
				break;
			}
		}

		if (scm_is_cipher_match(rsn.akm_suites, rsn.akm_suite_count,
					WLAN_RSN_SEL(WLAN_AKM_FT_SAE))) {
			if (match_any_akm ||
			    (filter_akm == WLAN_AUTH_TYPE_FT_SAE)) {
				neg_auth = WLAN_AUTH_TYPE_FT_SAE;
				match = true;
				break;
			}
		}

		if (scm_is_cipher_match(rsn.akm_suites, rsn.akm_suite_count,
					WLAN_RSN_SEL(
					WLAN_AKM_FT_SUITEB_EAP_SHA384))) {
			if (match_any_akm ||
			    (filter_akm ==
			     WLAN_AUTH_TYPE_FT_SUITEB_EAP_SHA384)) {
				neg_auth = WLAN_AUTH_TYPE_FT_SUITEB_EAP_SHA384;
				match = true;
				break;
			}
		}
	}

	if (!match) {
		scm_debug(QDF_MAC_ADDR_FMT" : akm suites didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (!filter->ignore_pmf_cap)
		match = scm_check_pmf_match(filter, db_entry, &rsn);

	if (match) {
		security->auth_type = neg_auth;
		security->mc_enc = neg_mccipher;
	}

	return match;
}

/**
 * scm_is_wpa_mcast_cipher_match() - match the wpa mcast cipher type with AP's
 * mcast cipher
 * @wpa: AP's WPA IE
 * @filter: scan filter
 * @neg_mccipher: negotiated mc cipher if matched.
 *
 * Return: true if mc cipher is negotiated
 */
static bool
scm_is_wpa_mcast_cipher_match(struct wlan_wpa_ie *wpa,
	struct scan_filter *filter, enum wlan_enc_type *neg_mccipher)
{
	int i;
	bool match;
	uint8_t cipher_type;

	if (!wpa || !neg_mccipher || !filter)
		return false;

	for (i = 0; i < filter->num_of_mc_enc_type; i++) {

		if (filter->mc_enc_type[i] == WLAN_ENCRYPT_TYPE_ANY) {
			/* Try the more secured ones first. */

			/* Check AES */
			cipher_type = WLAN_CSE_CCMP;
			match = scm_is_cipher_match(&wpa->mc_cipher, 1,
						    WLAN_WPA_SEL(cipher_type));
			if (match) {
				*neg_mccipher = WLAN_ENCRYPT_TYPE_AES;
				return true;
			}
			/* Check TKIP */
			cipher_type = WLAN_CSE_TKIP;
			match = scm_is_cipher_match(&wpa->mc_cipher, 1,
						    WLAN_WPA_SEL(cipher_type));
			if (match) {
				*neg_mccipher = WLAN_ENCRYPT_TYPE_TKIP;
				return true;
			}
		} else {
			cipher_type =
			     scm_get_cipher_suite_type(filter->mc_enc_type[i]);
			match = scm_is_cipher_match(&wpa->mc_cipher, 1,
						    WLAN_WPA_SEL(cipher_type));
			if (match) {
				*neg_mccipher = filter->mc_enc_type[i];
				return true;
			}
		}
	}

	return false;
}

/**
 * scm_is_wpa_security() - Check if scan entry support WPA security
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if WPA security else false
 */
static bool scm_is_wpa_security(struct scan_filter *filter,
	struct scan_cache_entry *db_entry,
	struct security_info *security)
{
	int i;
	QDF_STATUS status;
	uint8_t cipher_type;
	bool match_any_akm, match = false;
	enum wlan_auth_type neg_auth = WLAN_NUM_OF_SUPPORT_AUTH_TYPE;
	enum wlan_enc_type neg_mccipher = WLAN_ENCRYPT_TYPE_NONE;
	struct wlan_wpa_ie wpa = {0};

	if (!security)
		return false;
	if (!util_scan_entry_wpa(db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT" : AP doesn't have WPA IE",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	status = wlan_parse_wpa_ie(util_scan_entry_wpa(db_entry), &wpa);
	if (QDF_IS_STATUS_ERROR(status)) {
		scm_err("failed to parse WPA IE, status %d", status);
		scm_hex_dump(QDF_TRACE_LEVEL_DEBUG,
			     util_scan_entry_wpa(db_entry),
			     util_scan_get_wpa_len(db_entry));
		return false;
	}

	cipher_type =
		scm_get_cipher_suite_type(security->uc_enc);
	match = scm_is_cipher_match(wpa.uc_ciphers,
		wpa.uc_cipher_count, WLAN_WPA_SEL(cipher_type));
	if (!match) {
		scm_debug(QDF_MAC_ADDR_FMT" : unicase cipher didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	match = scm_is_wpa_mcast_cipher_match(&wpa, filter, &neg_mccipher);
	if (!match) {
		scm_debug(QDF_MAC_ADDR_FMT" : mcast cipher didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	/* Initializing with false as it has true value already */
	match = false;
	for (i = 0; i < filter->num_of_auth; i++) {

		if (filter->auth_type[i] == WLAN_AUTH_TYPE_ANY)
			match_any_akm = true;
		else
			match_any_akm = false;
		/*
		 * Ciphers are supported, Match authentication algorithm and
		 * pick first matching authtype.
		 */
		/**/
		if (scm_is_cipher_match(wpa.auth_suites,
		   wpa.auth_suite_count,
		   WLAN_WPA_SEL(WLAN_AKM_IEEE8021X))) {
			if (match_any_akm || (WLAN_AUTH_TYPE_WPA ==
			    filter->auth_type[i])) {
				neg_auth = WLAN_AUTH_TYPE_WPA;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(wpa.auth_suites,
		   wpa.auth_suite_count,
		   WLAN_WPA_SEL(WLAN_AKM_PSK))) {
			if (match_any_akm || (WLAN_AUTH_TYPE_WPA_PSK ==
			    filter->auth_type[i])) {
				neg_auth = WLAN_AUTH_TYPE_WPA_PSK;
				match = true;
				break;
			}
		}
		if (scm_is_cipher_match(wpa.auth_suites,
		   wpa.auth_suite_count,
		   WLAN_WPA_CCKM_AKM)) {
			if (match_any_akm || (WLAN_AUTH_TYPE_CCKM_WPA ==
			    filter->auth_type[i])) {
				neg_auth = WLAN_AUTH_TYPE_CCKM_WPA;
				match = true;
				break;
			}
		}
	}

	if (!match)
		scm_debug(QDF_MAC_ADDR_FMT" : akm didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));

	if (match) {
		security->auth_type = neg_auth;
		security->mc_enc = neg_mccipher;
	}

	return match;
}

/**
 * scm_is_wapi_security() - Check if scan entry support WAPI security
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if WAPI security else false
 */
static bool scm_is_wapi_security(struct scan_filter *filter,
	struct scan_cache_entry *db_entry,
	struct security_info *security)
{
	int i;
	uint8_t cipher_type;
	bool match = false;
	enum wlan_auth_type neg_auth = WLAN_NUM_OF_SUPPORT_AUTH_TYPE;
	enum wlan_enc_type neg_mccipher = WLAN_ENCRYPT_TYPE_NONE;
	struct wlan_wapi_ie wapi = {0};

	if (!security)
		return false;
	if (!util_scan_entry_wapi(db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT" : mcast cipher didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	wlan_parse_wapi_ie(
		   util_scan_entry_wapi(db_entry), &wapi);

	cipher_type =
		scm_get_cipher_suite_type(security->uc_enc);
	match = scm_is_cipher_match(wapi.uc_cipher_suites,
		wapi.uc_cipher_count, WLAN_WAPI_SEL(cipher_type));
	if (!match) {
		scm_debug(QDF_MAC_ADDR_FMT" : unicast cipher didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	for (i = 0; i < filter->num_of_mc_enc_type; i++) {
		cipher_type =
		  scm_get_cipher_suite_type(
		  filter->mc_enc_type[i]);
		match = scm_is_cipher_match(&wapi.mc_cipher_suite,
				  1, WLAN_WAPI_SEL(cipher_type));
		if (match)
			break;
	}
	if (!match) {
		scm_debug(QDF_MAC_ADDR_FMT" : mcast cipher didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}
	neg_mccipher = filter->mc_enc_type[i];

	if (scm_is_cipher_match(wapi.akm_suites,
	   wapi.akm_suite_count,
	   WLAN_WAPI_SEL(WLAN_WAI_CERT_OR_SMS4))) {
		neg_auth =
			WLAN_AUTH_TYPE_WAPI_WAI_CERTIFICATE;
	} else if (scm_is_cipher_match(wapi.akm_suites,
	   wapi.akm_suite_count, WLAN_WAPI_SEL(WLAN_WAI_PSK))) {
		neg_auth = WLAN_AUTH_TYPE_WAPI_WAI_PSK;
	} else {
		scm_debug(QDF_MAC_ADDR_FMT" : akm is not supported",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	match = false;
	for (i = 0; i < filter->num_of_auth; i++) {
		if (filter->auth_type[i] == neg_auth) {
			match = true;
			break;
		}
	}

	if (!match)
		scm_debug(QDF_MAC_ADDR_FMT" : akm suite didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
	if (match) {
		security->auth_type = neg_auth;
		security->mc_enc = neg_mccipher;
	}

	return match;
}

/**
 * scm_is_def_security() - Check if any security in filter match
 * @filter: scan filter
 * @db_entry: db entry
 * @security: matched security.
 *
 * Return: true if any security else false
 */
static bool scm_is_def_security(struct scan_filter *filter,
				struct scan_cache_entry *db_entry,
				struct security_info *security)
{

	/* It is allowed to match anything. Try the more secured ones first. */
	/* Check GCMP_256 first */
	security->uc_enc = WLAN_ENCRYPT_TYPE_AES_GCMP_256;
	if (scm_is_rsn_security(filter, db_entry, security))
		return true;

	/* Check GCMP */
	security->uc_enc = WLAN_ENCRYPT_TYPE_AES_GCMP;
	if (scm_is_rsn_security(filter, db_entry, security))
		return true;

	/* Check AES */
	security->uc_enc = WLAN_ENCRYPT_TYPE_AES;
	if (scm_is_rsn_security(filter, db_entry, security))
		return true;
	if (scm_is_wpa_security(filter, db_entry, security))
		return true;

	/* Check TKIP */
	security->uc_enc = WLAN_ENCRYPT_TYPE_TKIP;
	if (scm_is_rsn_security(filter, db_entry, security))
		return true;
	if (scm_is_wpa_security(filter, db_entry, security))
		return true;

	/* Check AES */
	security->uc_enc = WLAN_ENCRYPT_TYPE_AES;
	if (scm_is_wpa_security(filter, db_entry, security))
		return true;

	/* Check TKIP */
	security->uc_enc = WLAN_ENCRYPT_TYPE_TKIP;
	if (scm_is_wpa_security(filter, db_entry, security))
		return true;

	/* Check WAPI */
	security->uc_enc = WLAN_ENCRYPT_TYPE_WPI;
	if (scm_is_wapi_security(filter, db_entry, security))
		return true;

	security->uc_enc = WLAN_ENCRYPT_TYPE_WEP104;
	if (scm_is_wep_security(filter, db_entry, security))
		return true;
	security->uc_enc = WLAN_ENCRYPT_TYPE_WEP40;
	if (scm_is_wep_security(filter, db_entry, security))
		return true;
	security->uc_enc = WLAN_ENCRYPT_TYPE_WEP104_STATICKEY;
	if (scm_is_wep_security(filter, db_entry, security))
		return true;
	security->uc_enc = WLAN_ENCRYPT_TYPE_WEP40_STATICKEY;
	if (scm_is_wep_security(filter, db_entry, security))
		return true;

	/* It must be open and no enc */
	if (db_entry->cap_info.wlan_caps.privacy)
		return false;

	security->auth_type = WLAN_AUTH_TYPE_OPEN_SYSTEM;
	security->mc_enc = WLAN_ENCRYPT_TYPE_NONE;
	security->uc_enc = WLAN_ENCRYPT_TYPE_NONE;

	return true;
}

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
		(struct fils_indication_ie *) db_entry->ie_list.fils_indication;

	end_ptr = (uint8_t *)indication_ie + indication_ie->len + 2;

	data = indication_ie->variable_data;
	if (indication_ie->is_cache_id_present &&
	    (data + CACHE_IDENTIFIER_LEN) <= end_ptr)
		data += CACHE_IDENTIFIER_LEN;

	if (indication_ie->is_hessid_present &&
	    (data + HESSID_LEN) <= end_ptr)
		data += HESSID_LEN;

	for (i = 1; i <= indication_ie->realm_identifiers_cnt &&
	     (data + REAM_HASH_LEN) <= end_ptr; i++) {
		if (!qdf_mem_cmp(filter->fils_scan_filter.fils_realm,
				 data, REAM_HASH_LEN))
			return true;
		/* Max realm count reached */
		if (indication_ie->realm_identifiers_cnt == i)
			break;
		else
			data = data + REAM_HASH_LEN;
	}

	return false;
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
	struct security_info local_security = {0};

	if (!filter->num_of_enc_type)
		return true;

	for (i = 0; (i < filter->num_of_enc_type) &&
	    !match; i++) {

		local_security.uc_enc =
			filter->enc_type[i];

		switch (filter->enc_type[i]) {
		case WLAN_ENCRYPT_TYPE_NONE:
			match = scm_is_open_security(filter,
				    db_entry, &local_security);
			break;
		case WLAN_ENCRYPT_TYPE_WEP40_STATICKEY:
		case WLAN_ENCRYPT_TYPE_WEP104_STATICKEY:
		case WLAN_ENCRYPT_TYPE_WEP40:
		case WLAN_ENCRYPT_TYPE_WEP104:
			match = scm_is_wep_security(filter,
				    db_entry, &local_security);
			break;
		case WLAN_ENCRYPT_TYPE_TKIP:
		case WLAN_ENCRYPT_TYPE_AES:
		case WLAN_ENCRYPT_TYPE_AES_GCMP:
		case WLAN_ENCRYPT_TYPE_AES_GCMP_256:
			/* First check if there is a RSN match */
			match = scm_is_rsn_security(filter, db_entry,
						    &local_security);
			/* If not RSN, then check WPA match */
			if (!match)
				match = scm_is_wpa_security(filter,
				    db_entry, &local_security);
			break;
		case WLAN_ENCRYPT_TYPE_WPI:/* WAPI */
			match = scm_is_wapi_security(filter,
				    db_entry, &local_security);
			break;
		case WLAN_ENCRYPT_TYPE_ANY:
		default:
			match  = scm_is_def_security(filter, db_entry,
						     &local_security);
			break;
		}
	}

	if (match && security)
		qdf_mem_copy(security, &local_security, sizeof(*security));

	return match;
}

bool scm_filter_match(struct wlan_objmgr_psoc *psoc,
		      struct scan_cache_entry *db_entry,
		      struct scan_filter *filter,
		      struct security_info *security)
{
	int i;
	bool match = false;
	struct scan_default_params *def_param;
	struct wlan_country_ie *cc_ie;

	def_param = wlan_scan_psoc_get_def_params(psoc);
	if (!def_param)
		return false;

	if (filter->age_threshold && filter->age_threshold <
					util_scan_entry_age(db_entry))
		return false;

	if (filter->p2p_results && !db_entry->is_p2p)
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
	if (!match && util_scan_entry_is_hidden_ap(db_entry)) {
		for (i = 0; i < filter->num_of_auth; i++) {
			if (filter->auth_type[i] == WLAN_AUTH_TYPE_OWE) {
				match = true;
				break;
			}
		}
	}
	if (!match && filter->num_of_ssid)
		return false;

	match = false;
	/* TO do Fill p2p MAC*/
	for (i = 0; i < filter->num_of_bssid; i++) {
		if (util_is_bssid_match(&filter->bssid_list[i],
		   &db_entry->bssid)) {
			match = true;
			break;
		}
		/* TODO match p2p mac */
	}
	if (!match && filter->num_of_bssid)
		return false;

	match = false;
	for (i = 0; i < filter->num_of_channels; i++) {
		if (!filter->chan_freq_list[i] ||
		    filter->chan_freq_list[i] ==
		    db_entry->channel.chan_freq) {
			match = true;
			break;
		}
	}

	if (!match && filter->num_of_channels)
		return false;

	if (filter->rrm_measurement_filter)
		return true;

	/* TODO match phyMode */

	if (!filter->ignore_auth_enc_type &&
	    !scm_is_security_match(filter, db_entry, security)) {
		scm_debug(QDF_MAC_ADDR_FMT" : Ignore as security profile didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	if (!util_is_bss_type_match(filter->bss_type, db_entry->cap_info)) {
		scm_debug(QDF_MAC_ADDR_FMT" : Ignore as bss type didn't match cap_info %x bss_type %d",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  db_entry->cap_info.value,
			  filter->bss_type);
		return false;
	}

	/* TODO match rate set */

	if (filter->only_wmm_ap &&
	   !db_entry->ie_list.wmeinfo &&
	   !db_entry->ie_list.wmeparam) {
		scm_debug(QDF_MAC_ADDR_FMT" : Ignore as required wmeinfo and wme params not present",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	/* Match realm */
	if (!scm_is_fils_config_match(filter, db_entry)) {
		scm_debug(QDF_MAC_ADDR_FMT" :Ignore as fils config didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}

	cc_ie = util_scan_entry_country(db_entry);
	if (!util_country_code_match(filter->country, cc_ie)) {
		scm_debug(QDF_MAC_ADDR_FMT" : Ignore as country %.*s didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes),
			  2, filter->country);
		return false;
	}

	if (!util_mdie_match(filter->mobility_domain,
	   (struct rsn_mdie *)db_entry->ie_list.mdie)) {
		scm_debug(QDF_MAC_ADDR_FMT" : Ignore as mdie didn't match",
			  QDF_MAC_ADDR_REF(db_entry->bssid.bytes));
		return false;
	}
	return true;
}

/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

/*
 * DOC: contains MLO manager util api's
 */
#include <wlan_cmn.h>
#include <wlan_mlo_mgr_sta.h>
#include <wlan_cm_public_struct.h>
#include <wlan_mlo_mgr_main.h>
#include <wlan_cm_api.h>
#include "wlan_scan_api.h"
#include "qdf_types.h"
#include "utils_mlo.h"
#include "wlan_mlo_mgr_cmn.h"
#include "wlan_utility.h"

#ifdef WLAN_FEATURE_11BE_MLO

static uint8_t *util_find_eid(uint8_t eid, uint8_t *frame, qdf_size_t len)
{
	if (!frame)
		return NULL;

	while (len >= MIN_IE_LEN && len >= frame[TAG_LEN_POS] + MIN_IE_LEN) {
		if (frame[ID_POS] == eid)
			return frame;

		len -= frame[TAG_LEN_POS] + MIN_IE_LEN;
		frame += frame[TAG_LEN_POS] + MIN_IE_LEN;
	}

	return NULL;
}

static
uint8_t *util_find_extn_eid(uint8_t eid, uint8_t extn_eid,
			    uint8_t *frame, qdf_size_t len)
{
	if (!frame)
		return NULL;

	while (len > MIN_IE_LEN && len >= frame[TAG_LEN_POS] + MIN_IE_LEN) {
		if ((frame[ID_POS] == eid) &&
		    (frame[ELEM_ID_EXTN_POS] == extn_eid))
			return frame;

		len -= frame[TAG_LEN_POS] + MIN_IE_LEN;
		frame += frame[TAG_LEN_POS] + MIN_IE_LEN;
	}
	return NULL;
}

static QDF_STATUS
util_parse_multi_link_ctrl(uint8_t *mlieseqpayload,
			   qdf_size_t mlieseqpayloadlen,
			   uint8_t **link_info,
			   qdf_size_t *link_info_len)
{
	qdf_size_t parsed_payload_len;
	uint16_t mlcontrol;
	uint16_t presence_bm;
	uint16_t cinfo_len = 0;
	uint16_t exp_cinfo_len = 0;

	/* This helper returns the location(s) and length(s) of (sub)field(s)
	 * inferable after parsing the Multi Link element Control field. These
	 * location(s) and length(s) is/are in reference to the payload section
	 * of the Multi Link element (after defragmentation, if applicable).
	 * Here, the payload is the point after the element ID extension of the
	 * Multi Link element, and includes the payloads of all subsequent
	 * fragments (if any) but not the headers of those fragments.
	 *
	 * Currently, the helper returns the location and length of the Link
	 * Info field in the Multi Link element sequence. Other (sub)field(s)
	 * can be added later as required.
	 */

	if (!mlieseqpayload) {
		mlo_err("ML seq payload pointer is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlieseqpayloadlen) {
		mlo_err("ML seq payload len is 0");
		return QDF_STATUS_E_INVAL;
	}

	if (mlieseqpayloadlen < WLAN_ML_CTRL_SIZE) {
		mlo_err_rl("ML seq payload len %zu < ML Control size %u",
			   mlieseqpayloadlen, WLAN_ML_CTRL_SIZE);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len = 0;

	qdf_mem_copy(&mlcontrol, mlieseqpayload, WLAN_ML_CTRL_SIZE);
	mlcontrol = qdf_le16_to_cpu(mlcontrol);
	parsed_payload_len += WLAN_ML_CTRL_SIZE;

	presence_bm = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				   WLAN_ML_CTRL_PBM_BITS);

	if (mlieseqpayloadlen <
			(parsed_payload_len + WLAN_ML_BV_CINFO_LENGTH_SIZE)) {
		mlo_err_rl("ML seq payload len %zu insufficient for common info length size %u after parsed payload len %zu.",
			   mlieseqpayloadlen,
			   WLAN_ML_BV_CINFO_LENGTH_SIZE,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	cinfo_len = *(mlieseqpayload + parsed_payload_len);

	if (cinfo_len >
			(mlieseqpayloadlen - parsed_payload_len)) {
		mlo_err_rl("ML seq common info len %u larger than ML seq payload len %zu after parsed payload len %zu.",
			   cinfo_len,
			   mlieseqpayloadlen,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len += WLAN_ML_BV_CINFO_LENGTH_SIZE;

	if (mlieseqpayloadlen <
			(parsed_payload_len + QDF_MAC_ADDR_SIZE)) {
		mlo_err_rl("ML seq payload len %zu insufficient for MAC address size %u after parsed payload len %zu.",
			   mlieseqpayloadlen,
			   QDF_MAC_ADDR_SIZE,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len += QDF_MAC_ADDR_SIZE;

	/* Check if Link ID info is present */
	if (presence_bm & WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P) {
		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 WLAN_ML_BV_CINFO_LINKIDINFO_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for Link ID info size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   WLAN_ML_BV_CINFO_LINKIDINFO_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_BV_CINFO_LINKIDINFO_SIZE;
	}

	/* Check if BSS parameter change count is present */
	if (presence_bm & WLAN_ML_BV_CTRL_PBM_BSSPARAMCHANGECNT_P) {
		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 WLAN_ML_BSSPARAMCHNGCNT_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for BSS parameter change count size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   WLAN_ML_BSSPARAMCHNGCNT_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_BSSPARAMCHNGCNT_SIZE;
	}

	/* Check if Medium Sync Delay Info is present */
	if (presence_bm & WLAN_ML_BV_CTRL_PBM_MEDIUMSYNCDELAYINFO_P) {
		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for Medium Sync Delay Info size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE;
	}

	/* Check if EML cap is present */
	if (presence_bm & WLAN_ML_BV_CTRL_PBM_EMLCAP_P) {
		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 WLAN_ML_BV_CINFO_EMLCAP_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for EML cap size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   WLAN_ML_BV_CINFO_EMLCAP_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_BV_CINFO_EMLCAP_SIZE;
	}

	/* Check if MLD cap is present */
	if (presence_bm & WLAN_ML_BV_CTRL_PBM_MLDCAPANDOP_P) {
		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 WLAN_ML_BV_CINFO_MLDCAPANDOP_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for MLD cap size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   WLAN_ML_BV_CINFO_MLDCAPANDOP_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_BV_CINFO_MLDCAPANDOP_SIZE;
	}

	/* Check if MLD ID is present */
	if (presence_bm & WLAN_ML_BV_CTRL_PBM_MLDID_P) {
		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 WLAN_ML_BV_CINFO_MLDID_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for MLD ID size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   WLAN_ML_BV_CINFO_MLDID_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_BV_CINFO_MLDID_SIZE;
	}

	/* Check if Ext MLD CAP OP is present */
	if (presence_bm & WLAN_ML_BV_CTRL_PBM_EXT_MLDCAPANDOP_P) {
		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 WLAN_ML_BV_CINFO_EXT_MLDCAPANDOP_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for Ext MLD CAP OP size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   WLAN_ML_BV_CINFO_EXT_MLDCAPANDOP_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_BV_CINFO_EXT_MLDCAPANDOP_SIZE;
	}

	exp_cinfo_len = parsed_payload_len - WLAN_ML_CTRL_SIZE;
	if (cinfo_len >= exp_cinfo_len) {
		/* If common info length received is greater,
		 *  skip through the additional bytes
		 */
		parsed_payload_len += (cinfo_len - exp_cinfo_len);
		mlo_debug("ML seq common info len %u, parsed payload length %zu, expected common info len %u",
			  cinfo_len, parsed_payload_len, exp_cinfo_len);
	} else {
		/* If cinfo_len < exp_cinfo_len return error */
		mlo_err_rl("ML seq common info len %u doesn't match with expected common info len %u",
			   cinfo_len, exp_cinfo_len);
		return QDF_STATUS_E_PROTO;
	}

	if (link_info_len) {
		*link_info_len = mlieseqpayloadlen - parsed_payload_len;
		mlo_debug("link_info_len:%zu, parsed_payload_len:%zu",
			  *link_info_len, parsed_payload_len);
	}

	if (mlieseqpayloadlen == parsed_payload_len) {
		if (link_info)
			*link_info = NULL;
		return QDF_STATUS_SUCCESS;
	}

	if (link_info)
		*link_info = mlieseqpayload + parsed_payload_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_prv_multi_link_ctrl(uint8_t *mlieseqpayload,
			       qdf_size_t mlieseqpayloadlen,
			       uint8_t **link_info,
			       qdf_size_t *link_info_len)
{
	qdf_size_t parsed_payload_len;
	uint16_t mlcontrol;
	uint16_t presence_bm;
	uint16_t cinfo_len = 0;
	uint16_t exp_cinfo_len = 0;

	/* This helper returns the location(s) and length(s) of (sub)field(s)
	 * inferable after parsing the Multi Link element Control field. These
	 * location(s) and length(s) is/are in reference to the payload section
	 * of the Multi Link element (after defragmentation, if applicable).
	 * Here, the payload is the point after the element ID extension of the
	 * Multi Link element, and includes the payloads of all subsequent
	 * fragments (if any) but not the headers of those fragments.
	 *
	 * Currently, the helper returns the location and length of the Link
	 * Info field in the Multi Link element sequence. Other (sub)field(s)
	 * can be added later as required.
	 */

	if (!mlieseqpayload) {
		mlo_err("ML seq payload pointer is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlieseqpayloadlen) {
		mlo_err("ML seq payload len is 0");
		return QDF_STATUS_E_INVAL;
	}

	if (mlieseqpayloadlen < WLAN_ML_CTRL_SIZE) {
		mlo_err_rl("ML seq payload len %zu < ML Control size %u",
			   mlieseqpayloadlen, WLAN_ML_CTRL_SIZE);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len = 0;

	qdf_mem_copy(&mlcontrol, mlieseqpayload, WLAN_ML_CTRL_SIZE);
	mlcontrol = qdf_le16_to_cpu(mlcontrol);
	parsed_payload_len += WLAN_ML_CTRL_SIZE;

	presence_bm = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				   WLAN_ML_CTRL_PBM_BITS);

	if (mlieseqpayloadlen <
			(parsed_payload_len + WLAN_ML_PRV_CINFO_LENGTH_SIZE)) {
		mlo_err_rl("ML seq payload len %zu insufficient for common info length size %u after parsed payload len %zu.",
			   mlieseqpayloadlen,
			   WLAN_ML_PRV_CINFO_LENGTH_SIZE,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	cinfo_len = *(mlieseqpayload + parsed_payload_len);
	parsed_payload_len += WLAN_ML_PRV_CINFO_LENGTH_SIZE;

	/* Check if MLD ID is present */
	if (presence_bm & WLAN_ML_PRV_CTRL_PBM_MLDID_P) {
		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 WLAN_ML_PRV_CINFO_MLDID_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for MLD ID size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   WLAN_ML_PRV_CINFO_MLDID_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_PRV_CINFO_MLDID_SIZE;
	}

	exp_cinfo_len = parsed_payload_len - WLAN_ML_CTRL_SIZE;
	if (cinfo_len != exp_cinfo_len) {
		mlo_err_rl("ML seq common info len %u doesn't match with expected common info len %u",
			   cinfo_len, exp_cinfo_len);
		return QDF_STATUS_E_PROTO;
	}

	if (link_info_len) {
		*link_info_len = mlieseqpayloadlen - parsed_payload_len;
		mlo_debug("link_info_len:%zu, parsed_payload_len:%zu",
			  *link_info_len, parsed_payload_len);
	}

	if (mlieseqpayloadlen == parsed_payload_len) {
		mlo_debug("No Link Info field present");
		if (link_info)
			*link_info = NULL;
		return QDF_STATUS_SUCCESS;
	}

	if (link_info)
		*link_info = mlieseqpayload + parsed_payload_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_bvmlie_perstaprofile_stactrl(uint8_t *subelempayload,
					qdf_size_t subelempayloadlen,
					uint8_t *linkid,
					uint16_t *beaconinterval,
					bool *is_beaconinterval_valid,
					uint64_t *tsfoffset,
					bool *is_tsfoffset_valid,
					bool *is_complete_profile,
					bool *is_macaddr_valid,
					struct qdf_mac_addr *macaddr,
					bool is_staprof_reqd,
					uint8_t **staprof,
					qdf_size_t *staprof_len,
					struct mlo_nstr_info *nstr_info,
					bool *is_nstrlp_present)
{
	qdf_size_t parsed_payload_len = 0;
	uint16_t stacontrol;
	uint8_t completeprofile;
	uint8_t nstrlppresent;
	enum wlan_ml_bv_linfo_perstaprof_stactrl_nstrbmsz nstrbmsz;
	qdf_size_t nstrlpoffset = 0;
	uint8_t link_id;

	/* This helper returns the location(s) and where required, the length(s)
	 * of (sub)field(s) inferable after parsing the STA Control field in the
	 * per-STA profile subelement. These location(s) and length(s) is/are in
	 * reference to the payload section of the per-STA profile subelement
	 * (after defragmentation, if applicable).  Here, the payload is the
	 * point after the subelement length in the subelement, and includes the
	 * payloads of all subsequent fragments (if any) but not the headers of
	 * those fragments.
	 *
	 * Currently, the helper returns the link ID, MAC address, and STA
	 * profile. More (sub)fields can be added when required.
	 */

	if (!subelempayload) {
		mlo_err("Pointer to subelement payload is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!subelempayloadlen) {
		mlo_err("Length of subelement payload is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (subelempayloadlen < WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_SIZE) {
		mlo_err_rl("Subelement payload length %zu octets is smaller than STA control field of per-STA profile subelement %u octets",
			   subelempayloadlen,
			   WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_SIZE);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len = 0;

	qdf_mem_copy(&stacontrol,
		     subelempayload,
		     WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_SIZE);
	stacontrol = le16toh(stacontrol);
	parsed_payload_len += WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_SIZE;

	link_id = QDF_GET_BITS(stacontrol,
			       WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_LINKID_IDX,
			       WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_LINKID_BITS);
	if (linkid)
		*linkid = link_id;

	/* Check if this a complete profile */
	completeprofile = QDF_GET_BITS(stacontrol,
				       WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_IDX,
				       WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_BITS);

	if (completeprofile && is_complete_profile)
		*is_complete_profile = true;

	/* Check STA Info Length */
	if (subelempayloadlen <
		parsed_payload_len + WLAN_ML_BV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE) {
		mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain STA Info Length of size %u octets after parsed payload length of %zu octets.",
			   subelempayloadlen,
			   WLAN_ML_BV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len += WLAN_ML_BV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE;

	if (is_macaddr_valid)
		*is_macaddr_valid = false;

	/* Check STA MAC address present bit */
	if (QDF_GET_BITS(stacontrol,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_MACADDRP_IDX,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_MACADDRP_BITS)) {
		if (subelempayloadlen <
				(parsed_payload_len + QDF_MAC_ADDR_SIZE)) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain MAC address of size %u octets after parsed payload length of %zu octets.",
				   subelempayloadlen,
				   QDF_MAC_ADDR_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		if (macaddr) {
			qdf_mem_copy(macaddr->bytes,
				     subelempayload + parsed_payload_len,
				     QDF_MAC_ADDR_SIZE);

			mlo_nofl_debug("Copied MAC address: " QDF_MAC_ADDR_FMT,
				       QDF_MAC_ADDR_REF(macaddr->bytes));

			if (is_macaddr_valid)
				*is_macaddr_valid = true;
		}

		parsed_payload_len += QDF_MAC_ADDR_SIZE;
	}

	/* Check Beacon Interval present bit */
	if (QDF_GET_BITS(stacontrol,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BCNINTP_IDX,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BCNINTP_BITS)) {
		if (subelempayloadlen <
				(parsed_payload_len +
				 WLAN_BEACONINTERVAL_LEN)) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain Beacon Interval of size %u octets after parsed payload length of %zu octets.",
				   subelempayloadlen,
				   WLAN_BEACONINTERVAL_LEN,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		if (beaconinterval) {
			qdf_mem_copy(beaconinterval,
				     subelempayload + parsed_payload_len,
				     WLAN_BEACONINTERVAL_LEN);
			*beaconinterval = qdf_le16_to_cpu(*beaconinterval);

			if (is_beaconinterval_valid)
				*is_beaconinterval_valid = true;
		}
		parsed_payload_len += WLAN_BEACONINTERVAL_LEN;
	}

	/* Check TSF Offset present bit */
	if (QDF_GET_BITS(stacontrol,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_TSFOFFSETP_IDX,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_TSFOFFSETP_BITS)) {
		if (!completeprofile) {
			mlo_err_rl("TSF offset is expected only for complete profiles");
			return QDF_STATUS_E_PROTO;
		}

		if (subelempayloadlen <
				(parsed_payload_len +
				 WLAN_ML_TSF_OFFSET_SIZE)) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain TSF Offset of size %u octets after parsed payload length of %zu octets.",
				   subelempayloadlen,
				   WLAN_ML_TSF_OFFSET_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		if (tsfoffset) {
			qdf_mem_copy(tsfoffset,
				     subelempayload + parsed_payload_len,
				     WLAN_TIMESTAMP_LEN);
			*tsfoffset = qdf_le64_to_cpu(*tsfoffset);

			if (is_tsfoffset_valid)
				*is_tsfoffset_valid = true;
		}

		parsed_payload_len += WLAN_ML_TSF_OFFSET_SIZE;
	}

	/* Check DTIM Info present bit */
	if (QDF_GET_BITS(stacontrol,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_DTIMINFOP_IDX,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_DTIMINFOP_BITS)) {
		if (subelempayloadlen <
				(parsed_payload_len +
				 sizeof(struct wlan_ml_bv_linfo_perstaprof_stainfo_dtiminfo))) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain DTIM Info of size %zu octets after parsed payload length of %zu octets.",
				   subelempayloadlen,
				   sizeof(struct wlan_ml_bv_linfo_perstaprof_stainfo_dtiminfo),
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len +=
			sizeof(struct wlan_ml_bv_linfo_perstaprof_stainfo_dtiminfo);
	}

	/* Check NTSR Link pair present bit */
	nstrlppresent =
		QDF_GET_BITS(stacontrol,
			     WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRLINKPRP_IDX,
			     WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRLINKPRP_BITS);

	if (completeprofile && nstrlppresent) {
		nstrlpoffset = parsed_payload_len;
		/* Check NTSR Bitmap Size bit */
		nstrbmsz =
			QDF_GET_BITS(stacontrol,
				     WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_IDX,
				     WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_BITS);

		if (nstrbmsz == WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_1_OCTET) {
			if (subelempayloadlen <
					(parsed_payload_len + 1)) {
				mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain NTSR Bitmap of size 1 octet after parsed payload length of %zu octets.",
					   subelempayloadlen,
					   parsed_payload_len);
				return QDF_STATUS_E_PROTO;
			}

			parsed_payload_len += 1;
		} else if (nstrbmsz == WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_2_OCTETS) {
			if (subelempayloadlen <
					(parsed_payload_len + 2)) {
				mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain NTSR Bitmap  of size 2 octets after parsed payload length of %zu octets.",
					   subelempayloadlen,
					   parsed_payload_len);
				return QDF_STATUS_E_PROTO;
			}

			parsed_payload_len += 2;
		} else {
			/* Though an invalid value cannot occur if only 1 bit is
			 * used, we check for it in a generic manner in case the
			 * number of bits is increased in the future.
			 */
			mlo_err_rl("Invalid NSTR Bitmap size %u", nstrbmsz);
			return QDF_STATUS_E_PROTO;
		}
		if (nstr_info) {
			nstr_info->nstr_lp_present = nstrlppresent;
			nstr_info->nstr_bmp_size = nstrbmsz;
			*is_nstrlp_present = true;
			nstr_info->link_id = link_id;

			if (nstrbmsz == WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_1_OCTET) {
				nstr_info->nstr_lp_bitmap =
					*(uint8_t *)(subelempayload + nstrlpoffset);
			} else if (nstrbmsz == WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_NSTRBMSZ_2_OCTETS) {
				nstr_info->nstr_lp_bitmap =
					qdf_le16_to_cpu(*(uint16_t *)(subelempayload + nstrlpoffset));
			}
		}
	}

	/* Check BSS Parameters Change Count Present bit */
	if (QDF_GET_BITS(stacontrol,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BSSPARAMCHNGCNTP_IDX,
			 WLAN_ML_BV_LINFO_PERSTAPROF_STACTRL_BSSPARAMCHNGCNTP_BITS)) {
		if (subelempayloadlen <
				(parsed_payload_len +
				 WLAN_ML_BSSPARAMCHNGCNT_SIZE)) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain BSS Parameters Change Count of size %u octets after parsed payload length of %zu octets.",
				   subelempayloadlen,
				   WLAN_ML_BSSPARAMCHNGCNT_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += WLAN_ML_BSSPARAMCHNGCNT_SIZE;
	}

	/* Note: Some implementation versions of hostapd/wpa_supplicant may
	 * provide a per-STA profile without STA profile. Let the caller
	 * indicate whether a STA profile is required to be found. This may be
	 * revisited as upstreaming progresses.
	 */
	if (!is_staprof_reqd)
		return QDF_STATUS_SUCCESS;

	if (subelempayloadlen == parsed_payload_len) {
		mlo_err_rl("Subelement payload length %zu == parsed payload length %zu. Unable to get STA profile.",
			   subelempayloadlen,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	if (staprof_len)
		*staprof_len = subelempayloadlen - parsed_payload_len;

	if (staprof)
		*staprof = subelempayload + parsed_payload_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_prvmlie_perstaprofile_stactrl(uint8_t *subelempayload,
					 qdf_size_t subelempayloadlen,
					 uint8_t *linkid,
					 bool is_staprof_reqd,
					 uint8_t **staprof,
					 qdf_size_t *staprof_len)
{
	qdf_size_t parsed_payload_len = 0;
	uint16_t stacontrol;
	uint8_t completeprofile;

	/* This helper returns the location(s) and where required, the length(s)
	 * of (sub)field(s) inferable after parsing the STA Control field in the
	 * per-STA profile subelement. These location(s) and length(s) is/are in
	 * reference to the payload section of the per-STA profile subelement
	 * (after defragmentation, if applicable).  Here, the payload is the
	 * point after the subelement length in the subelement, and includes the
	 * payloads of all subsequent fragments (if any) but not the headers of
	 * those fragments.
	 *
	 * Currently, the helper returns the link ID, MAC address, and STA
	 * profile. More (sub)fields can be added when required.
	 */

	if (!subelempayload) {
		mlo_err("Pointer to subelement payload is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!subelempayloadlen) {
		mlo_err("Length of subelement payload is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (subelempayloadlen < WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_SIZE) {
		mlo_err_rl("Subelement payload length %zu octets is smaller than STA control field of per-STA profile subelement %u octets",
			   subelempayloadlen,
			   WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_SIZE);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len = 0;

	qdf_mem_copy(&stacontrol,
		     subelempayload,
		     WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_SIZE);
	stacontrol = qdf_le16_to_cpu(stacontrol);
	parsed_payload_len += WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_SIZE;

	if (linkid) {
		*linkid = QDF_GET_BITS(stacontrol,
				       WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_LINKID_IDX,
				       WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_LINKID_BITS);
	}

	/* Check if this a complete profile */
	completeprofile = QDF_GET_BITS(stacontrol,
				       WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_IDX,
				       WLAN_ML_PRV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_BITS);

	/* Note: Some implementation versions of hostapd/wpa_supplicant may
	 * provide a per-STA profile without STA profile. Let the caller
	 * indicate whether a STA profile is required to be found. This may be
	 * revisited as upstreaming progresses.
	 */
	if (!is_staprof_reqd)
		return QDF_STATUS_SUCCESS;

	if (subelempayloadlen == parsed_payload_len) {
		mlo_err_rl("Subelement payload length %zu == parsed payload length %zu. Unable to get STA profile.",
			   subelempayloadlen,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	if (staprof_len)
		*staprof_len = subelempayloadlen - parsed_payload_len;

	if (staprof)
		*staprof = subelempayload + parsed_payload_len;

	return QDF_STATUS_SUCCESS;
}

static
uint8_t *util_get_successorfrag(uint8_t *currie, uint8_t *frame, qdf_size_t len)
{
	uint8_t *nextie;

	if (!currie || !frame || !len)
		return NULL;

	if ((currie + MIN_IE_LEN) > (frame + len))
		return NULL;

	/* Check whether there is sufficient space in the frame for the current
	 * IE, plus at least another MIN_IE_LEN bytes for the IE header of a
	 * fragment (if present) that would come just after the current IE.
	 */
	if ((currie + MIN_IE_LEN + currie[TAG_LEN_POS] + MIN_IE_LEN) >
			(frame + len))
		return NULL;

	nextie = currie + currie[TAG_LEN_POS] + MIN_IE_LEN;

	/* Check whether there is sufficient space in the frame for the next IE
	 */
	if ((nextie + MIN_IE_LEN + nextie[TAG_LEN_POS]) > (frame + len))
		return NULL;

	if (nextie[ID_POS] != WLAN_ELEMID_FRAGMENT)
		return NULL;

	return nextie;
}

static
QDF_STATUS util_parse_partner_info_from_linkinfo(uint8_t *linkinfo,
						 qdf_size_t linkinfo_len,
						 struct mlo_partner_info *partner_info)
{
	uint8_t linkid;
	struct qdf_mac_addr macaddr;
	struct mlo_nstr_info nstr_info = {0};
	bool is_macaddr_valid;
	uint8_t *linkinfo_currpos;
	qdf_size_t linkinfo_remlen;
	bool is_subelemfragseq;
	uint8_t subelemid;
	qdf_size_t subelemseqtotallen;
	qdf_size_t subelemseqpayloadlen;
	qdf_size_t defragpayload_len;
	QDF_STATUS ret;
	bool is_nstrlp_present = false;

	/* This helper function parses partner info from the per-STA profiles
	 * present (if any) in the Link Info field in the payload of a Multi
	 * Link element (after defragmentation if required). The caller should
	 * pass a copy of the payload so that inline defragmentation of
	 * subelements can be carried out if required. The subelement
	 * defragmentation (if applicable) in this Control Path helper is
	 * required for maintainability, accuracy and eliminating current and
	 * future per-field-access multi-level fragment boundary checks and
	 * adjustments, given the complex format of Multi Link elements. It is
	 * also most likely to be required mainly at the client side.
	 */

	if (!linkinfo) {
		mlo_err("linkinfo is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!linkinfo_len) {
		mlo_err("linkinfo_len is zero");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!partner_info) {
		mlo_err("ML partner info is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	partner_info->num_partner_links = 0;
	linkinfo_currpos = linkinfo;
	linkinfo_remlen = linkinfo_len;

	while (linkinfo_remlen) {
		if (linkinfo_remlen <  sizeof(struct subelem_header)) {
			mlo_err_rl("Remaining length in link info %zu octets is smaller than subelement header length %zu octets",
				   linkinfo_remlen,
				   sizeof(struct subelem_header));
			return QDF_STATUS_E_PROTO;
		}

		subelemid = linkinfo_currpos[ID_POS];
		is_subelemfragseq = false;
		subelemseqtotallen = 0;
		subelemseqpayloadlen = 0;

		ret = wlan_get_subelem_fragseq_info(WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
						    linkinfo_currpos,
						    linkinfo_remlen,
						    &is_subelemfragseq,
						    &subelemseqtotallen,
						    &subelemseqpayloadlen);
		if (QDF_IS_STATUS_ERROR(ret))
			return ret;

		if (is_subelemfragseq) {
			if (!subelemseqpayloadlen) {
				mlo_err_rl("Subelement fragment sequence payload is reported as 0, investigate");
				return QDF_STATUS_E_FAILURE;
			}

			mlo_debug("Subelement fragment sequence found with payload len %zu",
				  subelemseqpayloadlen);

			ret = wlan_defrag_subelem_fragseq(true,
							  WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
							  linkinfo_currpos,
							  linkinfo_remlen,
							  NULL,
							  0,
							  &defragpayload_len);
			if (QDF_IS_STATUS_ERROR(ret))
				return ret;

			if (defragpayload_len != subelemseqpayloadlen) {
				mlo_err_rl("Length of defragmented payload %zu octets is not equal to length of subelement fragment sequence payload %zu octets",
					   defragpayload_len,
					   subelemseqpayloadlen);
				return QDF_STATUS_E_FAILURE;
			}

			/* Adjust linkinfo_remlen to reflect removal of all
			 * subelement headers except the header of the lead
			 * subelement.
			 */
			linkinfo_remlen -= (subelemseqtotallen -
					    subelemseqpayloadlen -
					    sizeof(struct subelem_header));
		} else {
			if (linkinfo_remlen <
				(sizeof(struct subelem_header) +
				 linkinfo_currpos[TAG_LEN_POS])) {
				mlo_err_rl("Remaining length in link info %zu octets is smaller than total size of current subelement %zu octets",
					   linkinfo_remlen,
					   sizeof(struct subelem_header) +
					   linkinfo_currpos[TAG_LEN_POS]);
				return QDF_STATUS_E_PROTO;
			}

			subelemseqpayloadlen = linkinfo_currpos[TAG_LEN_POS];
		}

		if (subelemid == WLAN_ML_LINFO_SUBELEMID_PERSTAPROFILE) {
			is_macaddr_valid = false;

			ret = util_parse_bvmlie_perstaprofile_stactrl(linkinfo_currpos +
								      sizeof(struct subelem_header),
								      subelemseqpayloadlen,
								      &linkid,
								      NULL,
								      NULL,
								      NULL,
								      NULL,
								      NULL,
								      &is_macaddr_valid,
								      &macaddr,
								      false,
								      NULL,
								      NULL,
								      &nstr_info,
								      &is_nstrlp_present);
			if (QDF_IS_STATUS_ERROR(ret)) {
				return ret;
			}

			if (is_nstrlp_present) {
				if (partner_info->num_nstr_info_links >=
					QDF_ARRAY_SIZE(partner_info->nstr_info)) {
					mlo_err_rl("Insufficient size %zu of array for nstr link info",
						   QDF_ARRAY_SIZE(partner_info->nstr_info));
					return QDF_STATUS_E_NOMEM;
				}
				qdf_mem_copy(&partner_info->nstr_info[partner_info->num_nstr_info_links],
					     &nstr_info, sizeof(nstr_info));
				partner_info->num_nstr_info_links++;
				is_nstrlp_present = false;
			}

			if (is_macaddr_valid) {
				if (partner_info->num_partner_links >=
					QDF_ARRAY_SIZE(partner_info->partner_link_info)) {
					mlo_err_rl("Insufficient size %zu of array for partner link info",
						   QDF_ARRAY_SIZE(partner_info->partner_link_info));
					return QDF_STATUS_E_NOMEM;
				}

				partner_info->partner_link_info[partner_info->num_partner_links].link_id =
					linkid;
				qdf_mem_copy(&partner_info->partner_link_info[partner_info->num_partner_links].link_addr,
					     &macaddr,
					     sizeof(partner_info->partner_link_info[partner_info->num_partner_links].link_addr));

				partner_info->num_partner_links++;
			} else {
				mlo_warn_rl("MAC address not found in STA Info field of per-STA profile with link ID %u",
					    linkid);
			}
		}

		linkinfo_remlen -= (sizeof(struct subelem_header) +
				    subelemseqpayloadlen);
		linkinfo_currpos += (sizeof(struct subelem_header) +
				     subelemseqpayloadlen);
	}

	mlo_debug("Number of ML partner links found=%u",
		  partner_info->num_partner_links);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_probereq_info_from_linkinfo(uint8_t *linkinfo,
				       qdf_size_t linkinfo_len,
				       struct mlo_probereq_info *probereq_info)
{
	uint8_t linkid;
	uint8_t *linkinfo_currpos;
	qdf_size_t linkinfo_remlen;
	bool is_subelemfragseq;
	uint8_t subelemid;
	qdf_size_t subelemseqtotallen;
	qdf_size_t subelemseqpayloadlen;
	qdf_size_t defragpayload_len;
	QDF_STATUS ret;

	/* This helper function parses probe request info from the per-STA prof
	 * present (if any) in the Link Info field in the payload of a Multi
	 * Link element (after defragmentation if required). The caller should
	 * pass a copy of the payload so that inline defragmentation of
	 * subelements can be carried out if required. The subelement
	 * defragmentation (if applicable) in this Control Path helper is
	 * required for maintainability, accuracy and eliminating current and
	 * future per-field-access multi-level fragment boundary checks and
	 * adjustments, given the complex format of Multi Link elements. It is
	 * also most likely to be required mainly at the client side.
	 */

	if (!linkinfo) {
		mlo_err("linkinfo is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!linkinfo_len) {
		mlo_err("linkinfo_len is zero");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!probereq_info) {
		mlo_err("ML probe req info is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	probereq_info->num_links = 0;
	linkinfo_currpos = linkinfo;
	linkinfo_remlen = linkinfo_len;

	while (linkinfo_remlen) {
		if (linkinfo_remlen <  sizeof(struct subelem_header)) {
			mlo_err_rl("Remaining length in link info %zu octets is smaller than subelement header length %zu octets",
				   linkinfo_remlen,
				   sizeof(struct subelem_header));
			return QDF_STATUS_E_PROTO;
		}

		subelemid = linkinfo_currpos[ID_POS];
		is_subelemfragseq = false;
		subelemseqtotallen = 0;
		subelemseqpayloadlen = 0;

		ret = wlan_get_subelem_fragseq_info(WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
						    linkinfo_currpos,
						    linkinfo_remlen,
						    &is_subelemfragseq,
						    &subelemseqtotallen,
						    &subelemseqpayloadlen);
		if (QDF_IS_STATUS_ERROR(ret))
			return ret;

		if (is_subelemfragseq) {
			if (!subelemseqpayloadlen) {
				mlo_err_rl("Subelement fragment sequence payload is reported as 0, investigate");
				return QDF_STATUS_E_FAILURE;
			}

			mlo_debug("Subelement fragment sequence found with payload len %zu",
				  subelemseqpayloadlen);

			ret = wlan_defrag_subelem_fragseq(true,
							  WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
							  linkinfo_currpos,
							  linkinfo_remlen,
							  NULL,
							  0,
							  &defragpayload_len);
			if (QDF_IS_STATUS_ERROR(ret))
				return ret;

			if (defragpayload_len != subelemseqpayloadlen) {
				mlo_err_rl("Length of defragmented payload %zu octets is not equal to length of subelement fragment sequence payload %zu octets",
					   defragpayload_len,
					   subelemseqpayloadlen);
				return QDF_STATUS_E_FAILURE;
			}

			/* Adjust linkinfo_remlen to reflect removal of all
			 * subelement headers except the header of the lead
			 * subelement.
			 */
			linkinfo_remlen -= (subelemseqtotallen -
					    subelemseqpayloadlen -
					    sizeof(struct subelem_header));
		} else {
			if (linkinfo_remlen <
				(sizeof(struct subelem_header) +
				 linkinfo_currpos[TAG_LEN_POS])) {
				mlo_err_rl("Remaining length in link info %zu octets is smaller than total size of current subelement %zu octets",
					   linkinfo_remlen,
					   sizeof(struct subelem_header) +
					   linkinfo_currpos[TAG_LEN_POS]);
				return QDF_STATUS_E_PROTO;
			}

			subelemseqpayloadlen = linkinfo_currpos[TAG_LEN_POS];
		}

		if (subelemid == WLAN_ML_LINFO_SUBELEMID_PERSTAPROFILE) {
			ret = util_parse_prvmlie_perstaprofile_stactrl(linkinfo_currpos +
								      sizeof(struct subelem_header),
								      subelemseqpayloadlen,
								      &linkid,
								      false,
								      NULL,
								      NULL);
			if (QDF_IS_STATUS_ERROR(ret))
				return ret;

			if (probereq_info->num_links >=
				QDF_ARRAY_SIZE(probereq_info->link_id)) {
				mlo_err_rl("Insufficient size %zu of array for probe req link id",
					   QDF_ARRAY_SIZE(probereq_info->link_id));
				return QDF_STATUS_E_NOMEM;
			}

			probereq_info->link_id[probereq_info->num_links] = linkid;

			probereq_info->num_links++;
			mlo_debug("LINK ID requested is = %u", linkid);
		}

		linkinfo_remlen -= (sizeof(struct subelem_header) +
				    subelemseqpayloadlen);
		linkinfo_currpos += (sizeof(struct subelem_header) +
				     subelemseqpayloadlen);
	}

	mlo_debug("Number of ML probe request links found=%u",
		  probereq_info->num_links);

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS util_get_noninheritlists(uint8_t *buff, qdf_size_t buff_len,
				    uint8_t **ninherit_elemlist,
				    qdf_size_t *ninherit_elemlist_len,
				    uint8_t **ninherit_elemextlist,
				    qdf_size_t *ninherit_elemextlist_len)
{
	uint8_t *ninherit_ie;
	qdf_size_t unparsed_len;

	/* Note: This functionality provided by this helper may be combined with
	 * other, older non-inheritance parsing helper functionality and exposed
	 * as a common API as part of future efforts once the older
	 * functionality can be made generic.
	 */

	if (!buff) {
		mlo_err("Pointer to buffer for IEs is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!buff_len) {
		mlo_err("IE buffer length is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (!ninherit_elemlist) {
		mlo_err("Pointer to Non-Inheritance element ID list array is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!ninherit_elemlist_len) {
		mlo_err("Pointer to Non-Inheritance element ID list array length is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!ninherit_elemextlist) {
		mlo_err("Pointer to Non-Inheritance element ID extension list array is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!ninherit_elemextlist_len) {
		mlo_err("Pointer to Non-Inheritance element ID extension list array length is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ninherit_ie = NULL;
	*ninherit_elemlist_len = 0;
	*ninherit_elemlist = NULL;
	*ninherit_elemextlist_len = 0;
	*ninherit_elemextlist = NULL;

	ninherit_ie =
		(uint8_t *)util_find_extn_eid(WLAN_ELEMID_EXTN_ELEM,
					      WLAN_EXTN_ELEMID_NONINHERITANCE,
					      buff,
					      buff_len);

	if (ninherit_ie) {
		if ((ninherit_ie + TAG_LEN_POS) > (buff + buff_len - 1)) {
			mlo_err_rl("Position of length field of Non-Inheritance element would exceed IE buffer boundary");
			return QDF_STATUS_E_PROTO;
		}

		if ((ninherit_ie + ninherit_ie[TAG_LEN_POS] + MIN_IE_LEN) >
				(buff + buff_len)) {
			mlo_err_rl("Non-Inheritance element with total length %u would exceed IE buffer boundary",
				   ninherit_ie[TAG_LEN_POS] + MIN_IE_LEN);
			return QDF_STATUS_E_PROTO;
		}

		if ((ninherit_ie[TAG_LEN_POS] + MIN_IE_LEN) <
				MIN_NONINHERITANCEELEM_LEN) {
			mlo_err_rl("Non-Inheritance element size %u is smaller than the minimum required %u",
				   ninherit_ie[TAG_LEN_POS] + MIN_IE_LEN,
				   MIN_NONINHERITANCEELEM_LEN);
			return QDF_STATUS_E_PROTO;
		}

		/* Track the number of unparsed octets, excluding the IE header.
		 */
		unparsed_len = ninherit_ie[TAG_LEN_POS];

		/* Mark the element ID extension as parsed */
		unparsed_len--;

		*ninherit_elemlist_len = ninherit_ie[ELEM_ID_LIST_LEN_POS];
		unparsed_len--;

		/* While checking if the Non-Inheritance element ID list length
		 * exceeds the remaining unparsed IE space, we factor in one
		 * octet for the element extension ID list length and subtract
		 * this from the unparsed IE space.
		 */
		if (*ninherit_elemlist_len > (unparsed_len - 1)) {
			mlo_err_rl("Non-Inheritance element ID list length %zu exceeds remaining unparsed IE space, minus an octet for element extension ID list length %zu",
				   *ninherit_elemlist_len, unparsed_len - 1);

			return QDF_STATUS_E_PROTO;
		}

		if (*ninherit_elemlist_len != 0) {
			*ninherit_elemlist = ninherit_ie + ELEM_ID_LIST_POS;
			unparsed_len -= *ninherit_elemlist_len;
		}

		*ninherit_elemextlist_len =
			ninherit_ie[ELEM_ID_LIST_LEN_POS + *ninherit_elemlist_len + 1];
		unparsed_len--;

		if (*ninherit_elemextlist_len > unparsed_len) {
			mlo_err_rl("Non-Inheritance element ID extension list length %zu exceeds remaining unparsed IE space %zu",
				   *ninherit_elemextlist_len, unparsed_len);

			return QDF_STATUS_E_PROTO;
		}

		if (*ninherit_elemextlist_len != 0) {
			*ninherit_elemextlist = ninherit_ie +
				ELEM_ID_LIST_LEN_POS + (*ninherit_elemlist_len)
				+ 2;
			unparsed_len -= *ninherit_elemextlist_len;
		}

		if (unparsed_len > 0) {
			mlo_err_rl("Unparsed length is %zu, expected 0",
				   unparsed_len);
			return QDF_STATUS_E_PROTO;
		}
	}

	/* If Non-Inheritance element is not found, we still return success,
	 * with the list lengths kept at zero.
	 */
	mlo_debug("Non-Inheritance element ID list array length=%zu",
		  *ninherit_elemlist_len);
	mlo_debug("Non-Inheritance element ID extension list array length=%zu",
		  *ninherit_elemextlist_len);

	return QDF_STATUS_SUCCESS;
}

static
QDF_STATUS util_eval_ie_in_noninheritlist(uint8_t *ie, qdf_size_t total_ie_len,
					  uint8_t *ninherit_elemlist,
					  qdf_size_t ninherit_elemlist_len,
					  uint8_t *ninherit_elemextlist,
					  qdf_size_t ninherit_elemextlist_len,
					  bool *is_in_noninheritlist)
{
	int i;

	/* Evaluate whether the given IE is in the given Non-Inheritance element
	 * ID list or Non-Inheritance element ID extension list, and update the
	 * result into is_in_noninheritlist. If any list is empty, then the IE
	 * is considered to not be present in that list. Both lists can be
	 * empty.
	 *
	 * If QDF_STATUS_SUCCESS is returned, it means that the evaluation is
	 * successful, and that is_in_noninheritlist contains a valid value
	 * (which could be true or false). If a QDF_STATUS error value is
	 * returned, the value in is_in_noninheritlist is invalid and the caller
	 * should ignore it.
	 */

	/* Note: The functionality provided by this helper may be combined with
	 * other, older non-inheritance parsing helper functionality and exposed
	 * as a common API as part of future efforts once the older
	 * functionality can be made generic.
	 */

	/* Except for is_in_noninheritlist and ie, other pointer arguments are
	 * permitted to be NULL if they are inapplicable. If they are
	 * applicable, they will be checked to ensure they are not NULL.
	 */

	if (!is_in_noninheritlist) {
		mlo_err("NULL pointer to flag that indicates if element is in a Non-Inheritance list");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* If ninherit_elemlist_len and ninherit_elemextlist_len are both zero
	 * as checked soon in this function, we won't be accessing the IE.
	 * However, we still check right-away if the pointer to the IE is
	 * non-NULL and whether the total IE length is sane enough to access the
	 * element ID and if applicable, the element ID extension, since it
	 * doesn't make sense to set the flag in is_in_noninheritlist for a NULL
	 * IE pointer or an IE whose total length is not sane enough to
	 * distinguish the identity of the IE.
	 */
	if (!ie) {
		mlo_err("NULL pointer to IE");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (total_ie_len < (ID_POS + 1)) {
		mlo_err("Total IE length %zu is smaller than minimum required to access element ID %u",
			total_ie_len, ID_POS + 1);
		return QDF_STATUS_E_INVAL;
	}

	if ((ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) &&
	    (total_ie_len < (IDEXT_POS + 1))) {
		mlo_err("Total IE length %zu is smaller than minimum required to access element ID extension %u",
			total_ie_len, IDEXT_POS + 1);
		return QDF_STATUS_E_INVAL;
	}

	*is_in_noninheritlist = false;

	/* If both the Non-Inheritance element list and Non-Inheritance element
	 * ID extension list are empty, then return success since we can
	 * conclude immediately that the given element does not occur in any
	 * Non-Inheritance list. The is_in_noninheritlist remains set to false
	 * as required.
	 */
	if (!ninherit_elemlist_len && !ninherit_elemextlist_len)
		return QDF_STATUS_SUCCESS;

	if (ie[ID_POS] != WLAN_ELEMID_EXTN_ELEM) {
		if (!ninherit_elemlist_len)
			return QDF_STATUS_SUCCESS;

		if (!ninherit_elemlist) {
			mlo_err("NULL pointer to Non-Inheritance element ID list though length of element ID list is %zu",
				ninherit_elemlist_len);
			return QDF_STATUS_E_NULL_VALUE;
		}

		for (i = 0; i < ninherit_elemlist_len; i++) {
			if (ie[ID_POS] == ninherit_elemlist[i]) {
				*is_in_noninheritlist = true;
				return QDF_STATUS_SUCCESS;
			}
		}
	} else {
		if (!ninherit_elemextlist_len)
			return QDF_STATUS_SUCCESS;

		if (!ninherit_elemextlist) {
			mlo_err("NULL pointer to Non-Inheritance element ID extension list though length of element ID extension list is %zu",
				ninherit_elemextlist_len);
			return QDF_STATUS_E_NULL_VALUE;
		}

		for (i = 0; i < ninherit_elemextlist_len; i++) {
			if (ie[IDEXT_POS] == ninherit_elemextlist[i]) {
				*is_in_noninheritlist = true;
				return QDF_STATUS_SUCCESS;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}

#if defined (SAP_MULTI_LINK_EMULATION)
static inline
QDF_STATUS util_validate_reportingsta_ie(const uint8_t *reportingsta_ie,
					 const uint8_t *frame_iesection,
					 const qdf_size_t frame_iesection_len)
{
	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS util_validate_reportingsta_ie(const uint8_t *reportingsta_ie,
					 const uint8_t *frame_iesection,
					 const qdf_size_t frame_iesection_len)
{
	qdf_size_t reportingsta_ie_size;

	if (!reportingsta_ie) {
		mlo_err("Pointer to reporting STA IE is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!frame_iesection) {
		mlo_err("Pointer to start of IE section in reporting frame is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!frame_iesection_len) {
		mlo_err("Length of IE section in reporting frame is zero");
		return QDF_STATUS_E_INVAL;
	}

	if ((reportingsta_ie + ID_POS) > (frame_iesection +
			frame_iesection_len - 1)) {
		mlo_err_rl("Position of element ID field of element for reporting STA would exceed frame IE section boundary");
		return QDF_STATUS_E_PROTO;
	}

	if ((reportingsta_ie + TAG_LEN_POS) > (frame_iesection +
			frame_iesection_len - 1)) {
		mlo_err_rl("Position of length field of element with element ID %u for reporting STA would exceed frame IE section boundary",
			   reportingsta_ie[ID_POS]);
		return QDF_STATUS_E_PROTO;
	}

	if ((reportingsta_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) &&
	    ((reportingsta_ie + IDEXT_POS) > (frame_iesection +
				frame_iesection_len - 1))) {
		mlo_err_rl("Position of element ID extension field of element would exceed frame IE section boundary");
		return QDF_STATUS_E_PROTO;
	}

	reportingsta_ie_size = reportingsta_ie[TAG_LEN_POS] + MIN_IE_LEN;

	if ((reportingsta_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) &&
	    (reportingsta_ie_size < (IDEXT_POS + 1))) {
		mlo_err_rl("Total length %zu of element for reporting STA is smaller than minimum required to access element ID extension %u",
			   reportingsta_ie_size, IDEXT_POS + 1);
		return QDF_STATUS_E_PROTO;
	}

	if ((reportingsta_ie[ID_POS] == WLAN_ELEMID_VENDOR) &&
	    (reportingsta_ie_size < (PAYLOAD_START_POS + OUI_LEN))) {
		mlo_err_rl("Total length %zu of element for reporting STA is smaller than minimum required to access vendor EID %u",
			   reportingsta_ie_size, PAYLOAD_START_POS + OUI_LEN);
		return QDF_STATUS_E_PROTO;
	}

	if ((reportingsta_ie + reportingsta_ie_size) >
			(frame_iesection + frame_iesection_len)) {
		if (reportingsta_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) {
			mlo_err_rl("Total size %zu octets of element with element ID %u element ID extension %u for reporting STA would exceed frame IE section boundary",
				   reportingsta_ie_size,
				   reportingsta_ie[ID_POS],
				   reportingsta_ie[IDEXT_POS]);
		} else {
			mlo_err_rl("Total size %zu octets of element with element ID %u for reporting STA would exceed frame IE section boundary",
				   reportingsta_ie_size,
				   reportingsta_ie[ID_POS]);
		}

		return QDF_STATUS_E_PROTO;
	}

	return QDF_STATUS_SUCCESS;
}
#endif

static inline
QDF_STATUS util_validate_sta_prof_ie(const uint8_t *sta_prof_ie,
				     const uint8_t *sta_prof_iesection,
				     const qdf_size_t sta_prof_iesection_len)
{
	qdf_size_t sta_prof_ie_size;

	if (!sta_prof_ie) {
		mlo_err("Pointer to STA profile IE is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!sta_prof_iesection) {
		mlo_err("Pointer to start of IE section in STA profile is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!sta_prof_iesection_len) {
		mlo_err("Length of IE section in STA profile is zero");
		return QDF_STATUS_E_INVAL;
	}

	if ((sta_prof_ie + ID_POS) > (sta_prof_iesection +
			sta_prof_iesection_len - 1)) {
		mlo_err_rl("Position of element ID field of STA profile element would exceed STA profile IE section boundary");
		return QDF_STATUS_E_PROTO;
	}

	if ((sta_prof_ie + TAG_LEN_POS) > (sta_prof_iesection +
			sta_prof_iesection_len - 1)) {
		mlo_err_rl("Position of length field of element with element ID %u in STA profile would exceed STA profile IE section boundary",
			   sta_prof_ie[ID_POS]);
		return QDF_STATUS_E_PROTO;
	}

	if ((sta_prof_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) &&
	    ((sta_prof_ie + IDEXT_POS) > (sta_prof_iesection +
				sta_prof_iesection_len - 1))) {
		mlo_err_rl("Position of element ID extension field of element would exceed STA profile IE section boundary");
		return QDF_STATUS_E_PROTO;
	}

	sta_prof_ie_size = sta_prof_ie[TAG_LEN_POS] + MIN_IE_LEN;

	if ((sta_prof_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) &&
	    (sta_prof_ie_size < (IDEXT_POS + 1))) {
		mlo_err_rl("Total length %zu of STA profile element is smaller than minimum required to access element ID extension %u",
			   sta_prof_ie_size, IDEXT_POS + 1);
		return QDF_STATUS_E_PROTO;
	}

	if ((sta_prof_ie + sta_prof_ie_size) >
			(sta_prof_iesection + sta_prof_iesection_len)) {
		if (sta_prof_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) {
			mlo_err_rl("Total size %zu octets of element with element ID %u element ID extension %u in STA profile would exceed STA profile IE section boundary",
				   sta_prof_ie_size,
				   sta_prof_ie[ID_POS],
				   sta_prof_ie[IDEXT_POS]);
		} else {
			mlo_err_rl("Total size %zu octets of element with element ID %u in STA profile would exceed STA profile IE section boundary",
				   sta_prof_ie_size,
				   sta_prof_ie[ID_POS]);
		}

		return QDF_STATUS_E_PROTO;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef CONN_MGR_ADV_FEATURE
/**
 * util_add_mlie_for_prb_rsp_gen - Add the basic variant Multi-Link element
 * when generating link specific probe response.
 * @reportingsta_ie: Pointer to the reportingsta ie
 * @reportingsta_ie_len: Length for reporting sta ie
 * @plink_frame_currpos: Pointer to Link frame current pos
 * @plink_frame_currlen: Current length of link frame.
 * @link_frame_maxsize: Maximum size of the frame to be generated
 * @linkid: Link Id value
 *
 * Add the basic variant Multi-Link element when
 * generating link specific probe response.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
static QDF_STATUS
util_add_mlie_for_prb_rsp_gen(const uint8_t *reportingsta_ie,
			      qdf_size_t reportingsta_ie_len,
			      uint8_t **plink_frame_currpos,
			      qdf_size_t *plink_frame_currlen,
			      qdf_size_t link_frame_maxsize,
			      uint8_t linkid)
{
	uint8_t mlie_len = 0;
	uint8_t common_info_len = 0;
	struct wlan_ie_multilink ml_ie_ff;
	uint16_t mlcontrol;
	uint16_t presencebm;
	uint8_t *mlie_frame = NULL;
	uint8_t link_id_offset = sizeof(struct wlan_ie_multilink) +
				QDF_MAC_ADDR_SIZE +
				WLAN_ML_BV_CINFO_LENGTH_SIZE;
	uint8_t *link_frame_currpos = *plink_frame_currpos;
	qdf_size_t link_frame_currlen = *plink_frame_currlen;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	status = util_get_mlie_common_info_len((uint8_t *)reportingsta_ie,
					       reportingsta_ie_len,
					       &common_info_len);
	if (QDF_IS_STATUS_ERROR(status) ||
	    common_info_len > reportingsta_ie_len ||
	    (reportingsta_ie_len - common_info_len <
	     sizeof(struct wlan_ie_multilink))) {
		mlo_err("Failed to parse common info, mlie len %d common info len %d",
			reportingsta_ie_len, common_info_len);
		return status;
	}

	/* common info len + bvmlie fixed fields */
	mlie_len = common_info_len + sizeof(struct wlan_ie_multilink);

	mlo_debug_rl("mlie_len %d, common_info_len %d, link_id_offset %d",
		     mlie_len,
		     common_info_len,
		     link_id_offset);

	/*
	 * Validate the buffer available before copying ML IE.
	 * Incase if mlie_len is modified at later place, move this validation
	 * there to make sure no buffer overflow happens.
	 */
	if ((link_frame_maxsize - link_frame_currlen) < mlie_len) {
		mlo_err("Insufficient space in link specific frame for ML IE. Required: %u octets, available: %zu octets",
			mlie_len, (link_frame_maxsize - link_frame_currlen));
		return QDF_STATUS_E_NOMEM;
	}

	mlie_frame = qdf_mem_malloc(mlie_len);
	if (!mlie_frame)
		return QDF_STATUS_E_NOMEM;

	/* Copy ml ie fixed fields */
	qdf_mem_copy(&ml_ie_ff,
		     reportingsta_ie,
		     sizeof(struct wlan_ie_multilink));

	ml_ie_ff.elem_len = mlie_len - sizeof(struct ie_header);

	mlcontrol = qdf_le16_to_cpu(ml_ie_ff.mlcontrol);
	presencebm = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				  WLAN_ML_CTRL_PBM_BITS);
	qdf_set_bit(WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P,
		    (unsigned long *)&presencebm);

	QDF_SET_BITS(ml_ie_ff.mlcontrol,
		     WLAN_ML_CTRL_PBM_IDX,
		     WLAN_ML_CTRL_PBM_BITS,
		     presencebm);

	qdf_mem_copy(mlie_frame,
		     &ml_ie_ff,
		     sizeof(struct wlan_ie_multilink));

	qdf_mem_copy(mlie_frame + sizeof(struct wlan_ie_multilink),
		     reportingsta_ie + sizeof(struct wlan_ie_multilink),
		     mlie_len - sizeof(struct wlan_ie_multilink));

	if (linkid == 0xFF || mlie_len <= link_id_offset) {
		qdf_mem_free(mlie_frame);
		mlo_err("Failed to process link id, link_id %d", linkid);
		return QDF_STATUS_E_INVAL;
	}
	mlie_frame[link_id_offset] = (mlie_frame[link_id_offset] & ~0x0f) |
				   (linkid & 0x0f);
	qdf_mem_copy(link_frame_currpos,
		     mlie_frame,
		     mlie_len);

	mlo_debug("Add mlie for link id %d", linkid);
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   mlie_frame, mlie_len);

	link_frame_currpos += mlie_len;
	link_frame_currlen += mlie_len;
	*plink_frame_currpos = link_frame_currpos;
	*plink_frame_currlen = link_frame_currlen;
	qdf_mem_free(mlie_frame);

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS
util_add_mlie_for_prb_rsp_gen(const uint8_t *reportingsta_ie,
			      qdf_size_t reportingsta_ie_len,
			      uint8_t **plink_frame_currpos,
			      qdf_size_t *plink_frame_currlen,
			      qdf_size_t link_frame_maxsize,
			      uint8_t linkid)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * util_find_bvmlie_persta_prof_for_linkid() - get per sta profile per link id
 * @req_link_id: link id
 * @linkinfo: the pointer of link info
 * @linkinfo_len: the length of link info
 * @persta_prof_frame: the pointer to store the address of sta profile
 * @persta_prof_len: the sta profile length
 *
 * This helper function parses partner info from the per-STA profiles
 * present (if any) in the Link Info field in the payload of a Multi
 * Link element (after defragmentation if required). The caller should
 * pass a copy of the payload so that inline defragmentation of
 * subelements can be carried out if required. The subelement
 * defragmentation (if applicable) in this Control Path helper is
 * required for maintainability, accuracy and eliminating current and
 * future per-field-access multi-level fragment boundary checks and
 * adjustments, given the complex format of Multi Link elements. It is
 * also most likely to be required mainly at the client side.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
util_find_bvmlie_persta_prof_for_linkid(uint8_t req_link_id,
					uint8_t *linkinfo,
					qdf_size_t linkinfo_len,
					uint8_t **persta_prof_frame,
					qdf_size_t *persta_prof_len)
{
	uint8_t linkid;
	struct qdf_mac_addr macaddr;
	bool is_macaddr_valid;
	uint8_t *linkinfo_currpos;
	qdf_size_t linkinfo_remlen;
	bool is_subelemfragseq;
	uint8_t subelemid;
	qdf_size_t subelemseqtotallen;
	qdf_size_t subelemseqpayloadlen;
	qdf_size_t defragpayload_len;
	QDF_STATUS ret;

	if (!linkinfo) {
		mlo_err("linkinfo is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!linkinfo_len) {
		mlo_err("linkinfo_len is zero");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!persta_prof_frame) {
		mlo_err("Pointer to per-STA prof frame is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!persta_prof_len) {
		mlo_err("Length to per-STA prof frame is 0");
		return QDF_STATUS_E_NULL_VALUE;
	}

	linkinfo_currpos = linkinfo;
	linkinfo_remlen = linkinfo_len;

	while (linkinfo_remlen) {
		if (linkinfo_remlen <  sizeof(struct subelem_header)) {
			mlo_err_rl("Remaining length in link info %zu octets is smaller than subelement header length %zu octets",
				   linkinfo_remlen,
				   sizeof(struct subelem_header));
			return QDF_STATUS_E_PROTO;
		}

		subelemid = linkinfo_currpos[ID_POS];
		is_subelemfragseq = false;
		subelemseqtotallen = 0;
		subelemseqpayloadlen = 0;

		ret = wlan_get_subelem_fragseq_info(WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
						    linkinfo_currpos,
						    linkinfo_remlen,
						    &is_subelemfragseq,
						    &subelemseqtotallen,
						    &subelemseqpayloadlen);
		if (QDF_IS_STATUS_ERROR(ret))
			return ret;

		if (is_subelemfragseq) {
			if (!subelemseqpayloadlen) {
				mlo_err_rl("Subelement fragment sequence payload is reported as 0, investigate");
				return QDF_STATUS_E_FAILURE;
			}

			mlo_debug("Subelement fragment sequence found with payload len %zu",
				  subelemseqpayloadlen);

			ret = wlan_defrag_subelem_fragseq(true,
							  WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
							  linkinfo_currpos,
							  linkinfo_remlen,
							  NULL,
							  0,
							  &defragpayload_len);
			if (QDF_IS_STATUS_ERROR(ret))
				return ret;

			if (defragpayload_len != subelemseqpayloadlen) {
				mlo_err_rl("Length of defragmented payload %zu octets is not equal to length of subelement fragment sequence payload %zu octets",
					   defragpayload_len,
					   subelemseqpayloadlen);
				return QDF_STATUS_E_FAILURE;
			}

			/* Adjust linkinfo_remlen to reflect removal of all
			 * subelement headers except the header of the lead
			 * subelement.
			 */
			linkinfo_remlen -= (subelemseqtotallen -
					    subelemseqpayloadlen -
					    sizeof(struct subelem_header));
		} else {
			if (linkinfo_remlen <
				(sizeof(struct subelem_header) +
				 linkinfo_currpos[TAG_LEN_POS])) {
				mlo_err_rl("Remaining length in link info %zu octets is smaller than total size of current subelement %zu octets",
					   linkinfo_remlen,
					   sizeof(struct subelem_header) +
					   linkinfo_currpos[TAG_LEN_POS]);
				return QDF_STATUS_E_PROTO;
			}

			subelemseqpayloadlen = linkinfo_currpos[TAG_LEN_POS];
		}

		if (subelemid == WLAN_ML_LINFO_SUBELEMID_PERSTAPROFILE) {
			is_macaddr_valid = false;

			ret = util_parse_bvmlie_perstaprofile_stactrl(linkinfo_currpos +
								      sizeof(struct subelem_header),
								      subelemseqpayloadlen,
								      &linkid,
								      NULL,
								      NULL,
								      NULL,
								      NULL,
								      NULL,
								      &is_macaddr_valid,
								      &macaddr,
								      false,
								      NULL,
								      NULL,
								      NULL,
								      NULL);
			if (QDF_IS_STATUS_ERROR(ret))
				return ret;

			if (req_link_id == linkid) {
				mlo_debug("Found requested per-STA prof for linkid %u, len %zu",
					  linkid, subelemseqpayloadlen);
				*persta_prof_frame = linkinfo_currpos;
				*persta_prof_len = subelemseqpayloadlen;
				return QDF_STATUS_SUCCESS;
			}
		}

		linkinfo_remlen -= (sizeof(struct subelem_header) +
				    subelemseqpayloadlen);
		linkinfo_currpos += (sizeof(struct subelem_header) +
				     subelemseqpayloadlen);
	}

	return QDF_STATUS_E_PROTO;
}

static
QDF_STATUS util_gen_link_reqrsp_cmn(uint8_t *frame, qdf_size_t frame_len,
				    uint8_t subtype,
				    uint8_t req_link_id,
				    struct qdf_mac_addr link_addr,
				    uint8_t *link_frame,
				    qdf_size_t link_frame_maxsize,
				    qdf_size_t *link_frame_len)
{
	/* Please see documentation for util_gen_link_assoc_req() and
	 * util_gen_link_assoc_resp() for information on the inputs to and
	 * output from this helper, since those APIs are essentially wrappers
	 * over this helper.
	 */

	/* Pointer to Multi-Link element/Multi-Link element fragment sequence */
	uint8_t *mlieseq;
	/* Total length of Multi-Link element sequence (including fragments if
	 * any)
	 */
	qdf_size_t mlieseqlen;
	/* Variant (i.e. type) of the Multi-Link element */
	enum wlan_ml_variant variant;

	/* Length of the payload of the Multi-Link element (inclusive of
	 * fragment payloads if any) without IE headers and element ID extension
	 */
	qdf_size_t mlieseqpayloadlen;
	/* Pointer to copy of the payload of the Multi-Link element (inclusive
	 * of fragment payloads if any) without IE headers and element ID
	 * extension
	 */
	uint8_t *mlieseqpayload_copy;

	/* Pointer to start of Link Info within the copy of the payload of the
	 * Multi-Link element
	 */
	uint8_t *link_info;
	/* Length of the Link Info */
	qdf_size_t link_info_len;

	/* Pointer to the IE section that occurs after the fixed fields in the
	 * original frame for the reporting STA.
	 */
	uint8_t *frame_iesection;
	/* Offset to the start of the IE section in the original frame for the
	 * reporting STA.
	 */
	qdf_size_t frame_iesection_offset;
	/* Total length of the IE section in the original frame for the
	 * reporting STA.
	 */
	qdf_size_t frame_iesection_len;

	/* Pointer to the IEEE802.11 frame header in the link specific frame
	 * being generated for the reported STA.
	 */
	struct wlan_frame_hdr *link_frame_hdr;
	/* Current position in the link specific frame being generated for the
	 * reported STA.
	 */
	uint8_t *link_frame_currpos;
	/* Current length of the link specific frame being generated for the
	 * reported STA.
	 */
	qdf_size_t link_frame_currlen;

	/* Pointer to IE for reporting STA */
	const uint8_t *reportingsta_ie;
	/* Total size of IE for reporting STA, inclusive of the element header
	 */
	qdf_size_t reportingsta_ie_size;

	/* Pointer to current position in STA profile */
	uint8_t *sta_prof_currpos;
	/* Remaining length of STA profile */
	qdf_size_t sta_prof_remlen;
	/* Pointer to start of IE section in STA profile that occurs after fixed
	 * fields.
	 */
	uint8_t *sta_prof_iesection;
	/* Total length of IE section in STA profile */
	qdf_size_t sta_prof_iesection_len;
	/* Pointer to current position being processed in IE section in STA
	 * profile.
	 */
	uint8_t *sta_prof_iesection_currpos;
	/* Remaining length of IE section in STA profile */
	qdf_size_t sta_prof_iesection_remlen;

	/* Pointer to IE in STA profile, that occurs within IE section */
	uint8_t *sta_prof_ie;
	/* Total size of IE in STA profile, inclusive of the element header */
	qdf_size_t sta_prof_ie_size;

	/* Pointer to element ID list in Non-Inheritance IE */
	uint8_t *ninherit_elemlist;
	/* Length of element ID list in Non-Inheritance IE */
	qdf_size_t ninherit_elemlist_len;
	/* Pointer to element ID extension list in Non-Inheritance IE */
	uint8_t *ninherit_elemextlist;
	/* Length of element ID extension list in Non-Inheritance IE */
	qdf_size_t ninherit_elemextlist_len;
	/* Whether a given IE is in a non-inheritance list */
	bool is_in_noninheritlist;

	/* Whether MAC address of reported STA is valid */
	bool is_reportedmacaddr_valid;
	/* MAC address of reported STA */
	struct qdf_mac_addr reportedmacaddr;

	/* Pointer to per-STA profile */
	uint8_t *persta_prof;
	/* Length of the containing buffer which starts with the per-STA profile
	 */
	qdf_size_t persta_prof_bufflen;

	/* Other variables for temporary purposes */

	/* Variable into which API for determining fragment information will
	 * indicate whether the element is the start of a fragment sequence or
	 * not.
	 */
	bool is_elemfragseq;
	/*  De-fragmented payload length returned by API for element
	 *  defragmentation.
	 */
	qdf_size_t defragpayload_len;
	/* Pointer to Beacon interval in STA info field */
	uint16_t beaconinterval;
	/* Whether Beacon interval value valid */
	bool is_beaconinterval_valid;
	/* TSF timer of the reporting AP */
	uint64_t tsf;
	/* TSF offset of the reproted AP */
	uint64_t tsfoffset;
	/* TSF offset value valid */
	bool is_tsfoffset_valid;
	/* If Complete Profile or not*/
	bool is_completeprofile;
	qdf_size_t tmplen;
	QDF_STATUS ret;
	uint8_t linkid = 0xFF;

	if (!frame) {
		mlo_err("Pointer to original frame is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!frame_len) {
		mlo_err("Length of original frame is zero");
		return QDF_STATUS_E_INVAL;
	}

	if ((subtype != WLAN_FC0_STYPE_ASSOC_REQ) &&
	    (subtype != WLAN_FC0_STYPE_REASSOC_REQ) &&
	    (subtype != WLAN_FC0_STYPE_ASSOC_RESP) &&
	    (subtype != WLAN_FC0_STYPE_REASSOC_RESP) &&
	    (subtype != WLAN_FC0_STYPE_PROBE_RESP)) {
		mlo_err("802.11 frame subtype %u is invalid", subtype);
		return QDF_STATUS_E_INVAL;
	}

	if (!link_frame) {
		mlo_err("Pointer to secondary link specific frame is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!link_frame_maxsize) {
		mlo_err("Maximum size of secondary link specific frame is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (!link_frame_len) {
		mlo_err("Pointer to populated length of secondary link specific frame is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	frame_iesection_offset = 0;

	if (subtype == WLAN_FC0_STYPE_ASSOC_REQ) {
		frame_iesection_offset = WLAN_ASSOC_REQ_IES_OFFSET;
	} else if (subtype == WLAN_FC0_STYPE_REASSOC_REQ) {
		frame_iesection_offset = WLAN_REASSOC_REQ_IES_OFFSET;
	} else if (subtype == WLAN_FC0_STYPE_PROBE_RESP) {
		frame_iesection_offset = WLAN_PROBE_RESP_IES_OFFSET;
		if (frame_len < WLAN_TIMESTAMP_LEN) {
			mlo_err("Frame length %zu is smaller than required timestamp length",
				frame_len);
			return QDF_STATUS_E_INVAL;
		}
		qdf_mem_copy(&tsf, frame, WLAN_TIMESTAMP_LEN);
		tsf = qdf_le64_to_cpu(tsf);
	} else {
		/* This is a (re)association response */
		frame_iesection_offset = WLAN_ASSOC_RSP_IES_OFFSET;
	}

	if (frame_len < frame_iesection_offset) {
		/* The caller is supposed to have confirmed that this is a valid
		 * frame containing a Multi-Link element. Hence we treat this as
		 * a case of invalid argument being passed to us.
		 */
		mlo_err("Frame length %zu is smaller than the IE section offset %zu for subtype %u",
			frame_len, frame_iesection_offset, subtype);
		return QDF_STATUS_E_INVAL;
	}

	frame_iesection_len = frame_len - frame_iesection_offset;

	if (frame_iesection_len == 0) {
		/* The caller is supposed to have confirmed that this is a valid
		 * frame containing a Multi-Link element. Hence we treat this as
		 * a case of invalid argument being passed to us.
		 */
		mlo_err("No space left in frame for IE section");
		return QDF_STATUS_E_INVAL;
	}

	frame_iesection = frame + frame_iesection_offset;

	mlieseq = NULL;
	mlieseqlen = 0;

	ret = util_find_mlie(frame_iesection, frame_iesection_len, &mlieseq,
			     &mlieseqlen);
	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	if (!mlieseq) {
		/* The caller is supposed to have confirmed that a Multi-Link
		 * element is present in the frame. Hence we treat this as a
		 * case of invalid argument being passed to us.
		 */
		mlo_err("Invalid original frame since no Multi-Link element found");
		return QDF_STATUS_E_INVAL;
	}

	/* Sanity check the Multi-Link element sequence length */
	if (!mlieseqlen) {
		mlo_err("Length of Multi-Link element sequence is zero. Investigate.");
		return QDF_STATUS_E_FAILURE;
	}

	if (mlieseqlen < sizeof(struct wlan_ie_multilink)) {
		mlo_err_rl("Multi-Link element sequence length %zu octets is smaller than required for the fixed portion of Multi-Link element (%zu octets)",
			   mlieseqlen, sizeof(struct wlan_ie_multilink));
		return QDF_STATUS_E_PROTO;
	}

	ret = util_get_mlie_variant(mlieseq, mlieseqlen, (int *)&variant);
	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	if (variant != WLAN_ML_VARIANT_BASIC) {
		mlo_err_rl("Unexpected variant %u of Multi-Link element.",
			   variant);
		return QDF_STATUS_E_PROTO;
	}

	mlieseqpayloadlen = 0;
	tmplen = 0;
	is_elemfragseq = false;

	ret = wlan_get_elem_fragseq_info(mlieseq,
					 mlieseqlen,
					 &is_elemfragseq,
					 &tmplen,
					 &mlieseqpayloadlen);
	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	if (is_elemfragseq) {
		if (tmplen != mlieseqlen) {
			mlo_err_rl("Mismatch in values of element fragment sequence total length. Val per frag info determination: %zu octets, val per Multi-Link element search: %zu octets",
				   tmplen, mlieseqlen);
			return QDF_STATUS_E_FAILURE;
		}

		if (!mlieseqpayloadlen) {
			mlo_err_rl("Multi-Link element fragment sequence payload is reported as 0, investigate");
			return QDF_STATUS_E_FAILURE;
		}

		mlo_debug("ML IE fragment sequence found with payload len %zu",
			  mlieseqpayloadlen);
	} else {
		if (mlieseqlen > (sizeof(struct ie_header) + WLAN_MAX_IE_LEN)) {
			mlo_err_rl("Expected presence of valid fragment sequence since Multi-Link element sequence length %zu octets is larger than frag threshold of %zu octets, however no valid fragment sequence found",
				   mlieseqlen,
				   sizeof(struct ie_header) + WLAN_MAX_IE_LEN);
			return QDF_STATUS_E_FAILURE;
		}

		mlieseqpayloadlen = mlieseqlen - (sizeof(struct ie_header) + 1);
	}

	mlieseqpayload_copy = qdf_mem_malloc(mlieseqpayloadlen);

	if (!mlieseqpayload_copy) {
		mlo_err_rl("Could not allocate memory for Multi-Link element payload copy");
		ret = QDF_STATUS_E_NOMEM;
		goto mem_free;
	}

	if (is_elemfragseq) {
		ret = wlan_defrag_elem_fragseq(false,
					       mlieseq,
					       mlieseqlen,
					       mlieseqpayload_copy,
					       mlieseqpayloadlen,
					       &defragpayload_len);
		if (QDF_IS_STATUS_ERROR(ret))
			goto mem_free;

		if (defragpayload_len != mlieseqpayloadlen) {
			mlo_err_rl("Length of de-fragmented payload %zu octets is not equal to length of Multi-Link element fragment sequence payload %zu octets",
				   defragpayload_len, mlieseqpayloadlen);
			ret = QDF_STATUS_E_FAILURE;
			goto mem_free;
		}
	} else {
		qdf_mem_copy(mlieseqpayload_copy,
			     mlieseq + sizeof(struct ie_header) + 1,
			     mlieseqpayloadlen);
	}

	link_info = NULL;
	link_info_len = 0;

	ret = util_parse_multi_link_ctrl(mlieseqpayload_copy,
					 mlieseqpayloadlen,
					 &link_info,
					 &link_info_len);
	if (QDF_IS_STATUS_ERROR(ret))
		goto mem_free;

	/* As per the standard, the sender must include Link Info for
	 * association request/response. Throw an error if we are unable to
	 * obtain this.
	 */
	if (!link_info) {
		mlo_err_rl("Unable to successfully obtain Link Info");
		ret = QDF_STATUS_E_PROTO;
		goto mem_free;
	}

	/* Note: We may have a future change to skip subelements which are not
	 * Per-STA Profile, handle more than two links in MLO, handle cases
	 * where we unexpectedly find more Per-STA Profiles than expected, etc.
	 */

	persta_prof = NULL;
	persta_prof_bufflen = 0;

	ret = util_find_bvmlie_persta_prof_for_linkid(req_link_id,
						      link_info,
						      link_info_len,
						      &persta_prof,
						      &persta_prof_bufflen);

	if (QDF_IS_STATUS_ERROR(ret)) {
		mlo_err_rl("Per STA profile not found for link id %d",
			   req_link_id);
		goto mem_free;
	}

	sta_prof_remlen = 0;
	sta_prof_currpos = NULL;
	is_reportedmacaddr_valid = false;
	is_beaconinterval_valid = false;
	is_completeprofile = false;
	is_tsfoffset_valid = false;

	/* Parse per-STA profile */
	ret = util_parse_bvmlie_perstaprofile_stactrl(persta_prof +
						      sizeof(struct subelem_header),
						      persta_prof_bufflen,
						      &linkid,
						      &beaconinterval,
						      &is_beaconinterval_valid,
						      &tsfoffset,
						      &is_tsfoffset_valid,
						      &is_completeprofile,
						      &is_reportedmacaddr_valid,
						      &reportedmacaddr,
						      true,
						      &sta_prof_currpos,
						      &sta_prof_remlen,
						      NULL,
						      NULL);
	if (QDF_IS_STATUS_ERROR(ret))
		goto mem_free;

	if (subtype == WLAN_FC0_STYPE_PROBE_RESP && !is_completeprofile) {
		mlo_err("Complete profile information is not present in per-STA profile of probe response frame");
		ret = QDF_STATUS_E_NOSUPPORT;
		goto mem_free;
	}

	/* We double check for a NULL STA Profile, though the helper function
	 * above would have taken care of this. We need to get a non-NULL STA
	 * profile, because we need to get at least the expected fixed fields,
	 * even if there is an (improbable) total inheritance.
	 */
	if (!sta_prof_currpos) {
		mlo_err_rl("STA profile is NULL");
		ret = QDF_STATUS_E_PROTO;
		goto mem_free;
	}

	/* As per the standard, the sender sets the MAC address in the per-STA
	 * profile in association request/response. Without this, we cannot
	 * generate the link specific frame.
	 */
	if (!is_reportedmacaddr_valid) {
		mlo_err_rl("Unable to get MAC address from per-STA profile");
		ret = QDF_STATUS_E_PROTO;
		goto mem_free;
	}

	link_frame_currpos = link_frame;
	*link_frame_len = 0;
	link_frame_currlen = 0;

	if (link_frame_maxsize < WLAN_MAC_HDR_LEN_3A) {
		mlo_err("Insufficient space in link specific frame for 802.11 header. Required: %u octets, available: %zu octets",
			WLAN_MAC_HDR_LEN_3A, link_frame_maxsize);

		ret = QDF_STATUS_E_NOMEM;
		goto mem_free;
	}

	link_frame_currpos += WLAN_MAC_HDR_LEN_3A;
	link_frame_currlen += WLAN_MAC_HDR_LEN_3A;

	if ((subtype == WLAN_FC0_STYPE_ASSOC_REQ) ||
	    (subtype == WLAN_FC0_STYPE_REASSOC_REQ)) {
		mlo_debug("Populating fixed fields for (re)assoc req in link specific frame");

		if (sta_prof_remlen < WLAN_CAPABILITYINFO_LEN) {
			mlo_err_rl("Remaining length of STA profile %zu octets is less than length of Capability Info %u",
				   sta_prof_remlen,
				   WLAN_CAPABILITYINFO_LEN);

			ret = QDF_STATUS_E_PROTO;
			goto mem_free;
		}

		/* Capability information is specific to the link. Copy this
		 * from the STA profile.
		 */

		if ((link_frame_maxsize - link_frame_currlen) <
				WLAN_CAPABILITYINFO_LEN) {
			mlo_err("Insufficient space in link specific frame for Capability Info field. Required: %u octets, available: %zu octets",
				WLAN_CAPABILITYINFO_LEN,
				(link_frame_maxsize - link_frame_currlen));

			ret = QDF_STATUS_E_NOMEM;
			goto mem_free;
		}

		qdf_mem_copy(link_frame_currpos, sta_prof_currpos,
			     WLAN_CAPABILITYINFO_LEN);
		link_frame_currpos += WLAN_CAPABILITYINFO_LEN;
		link_frame_currlen += WLAN_CAPABILITYINFO_LEN;

		sta_prof_currpos += WLAN_CAPABILITYINFO_LEN;
		sta_prof_remlen -= WLAN_CAPABILITYINFO_LEN;

		/* Listen Interval is common between all links. Copy this from
		 * the reporting section of the frame.
		 */

		if ((link_frame_maxsize - link_frame_currlen) <
				WLAN_LISTENINTERVAL_LEN) {
			mlo_err("Insufficient space in link specific frame for Listen Interval field. Required: %u octets, available: %zu octets",
				WLAN_LISTENINTERVAL_LEN,
				(link_frame_maxsize - link_frame_currlen));

			ret = QDF_STATUS_E_NOMEM;
			goto mem_free;
		}

		qdf_mem_copy(link_frame_currpos,
			     frame + WLAN_CAPABILITYINFO_LEN,
			     WLAN_LISTENINTERVAL_LEN);
		link_frame_currpos += WLAN_LISTENINTERVAL_LEN;
		link_frame_currlen += WLAN_LISTENINTERVAL_LEN;

		if (subtype == WLAN_FC0_STYPE_REASSOC_REQ) {
			/* Current AP address is common between all links. Copy
			 * this from the reporting section of the frame.
			 */
			if ((link_frame_maxsize - link_frame_currlen) <
				QDF_MAC_ADDR_SIZE) {
				mlo_err("Insufficient space in link specific frame for current AP address. Required: %u octets, available: %zu octets",
					QDF_MAC_ADDR_SIZE,
					(link_frame_maxsize -
						link_frame_currlen));

				ret = QDF_STATUS_E_NOMEM;
				goto mem_free;
			}

			qdf_mem_copy(link_frame_currpos,
				     frame + WLAN_CAPABILITYINFO_LEN +
						WLAN_LISTENINTERVAL_LEN,
				     QDF_MAC_ADDR_SIZE);
			link_frame_currpos += QDF_MAC_ADDR_SIZE;
			link_frame_currlen += QDF_MAC_ADDR_SIZE;
			mlo_debug("Reassoc req: Added Current AP address field (%u octets) to link specific frame",
				  QDF_MAC_ADDR_SIZE);
		}
	} else if (subtype == WLAN_FC0_STYPE_ASSOC_RESP ||
		   subtype == WLAN_FC0_STYPE_REASSOC_RESP) {
		/* This is a (re)association response */
		mlo_debug("Populating fixed fields for (re)assoc resp in link specific frame");

		if (sta_prof_remlen <
			(WLAN_CAPABILITYINFO_LEN + WLAN_STATUSCODE_LEN)) {
			mlo_err_rl("Remaining length of STA profile %zu octets is less than length of Capability Info + length of Status Code %u",
				   sta_prof_remlen,
				   WLAN_CAPABILITYINFO_LEN +
					WLAN_STATUSCODE_LEN);

			ret = QDF_STATUS_E_PROTO;
			goto mem_free;
		}

		/* Capability information and Status Code are specific to the
		 * link. Copy these from the STA profile.
		 */

		if ((link_frame_maxsize - link_frame_currlen) <
			(WLAN_CAPABILITYINFO_LEN + WLAN_STATUSCODE_LEN)) {
			mlo_err("Insufficient space in link specific frame for Capability Info and Status Code fields. Required: %u octets, available: %zu octets",
				WLAN_CAPABILITYINFO_LEN + WLAN_STATUSCODE_LEN,
				(link_frame_maxsize - link_frame_currlen));

			ret = QDF_STATUS_E_NOMEM;
			goto mem_free;

		}

		qdf_mem_copy(link_frame_currpos, sta_prof_currpos,
			     (WLAN_CAPABILITYINFO_LEN + WLAN_STATUSCODE_LEN));
		link_frame_currpos += (WLAN_CAPABILITYINFO_LEN +
						WLAN_STATUSCODE_LEN);
		link_frame_currlen += (WLAN_CAPABILITYINFO_LEN +
				WLAN_STATUSCODE_LEN);
		mlo_debug("Added Capability Info and Status Code fields (%u octets) to link specific frame",
			  WLAN_CAPABILITYINFO_LEN + WLAN_STATUSCODE_LEN);

		sta_prof_currpos += (WLAN_CAPABILITYINFO_LEN +
				WLAN_STATUSCODE_LEN);
		sta_prof_remlen -= (WLAN_CAPABILITYINFO_LEN +
				WLAN_STATUSCODE_LEN);

		/* AID is common between all links. Copy this from the original
		 * frame.
		 */

		if ((link_frame_maxsize - link_frame_currlen) < WLAN_AID_LEN) {
			mlo_err("Insufficient space in link specific frame for AID field. Required: %u octets, available: %zu octets",
				WLAN_AID_LEN,
				(link_frame_maxsize - link_frame_currlen));

			ret = QDF_STATUS_E_NOMEM;
			goto mem_free;
		}

		qdf_mem_copy(link_frame_currpos,
			     frame + WLAN_CAPABILITYINFO_LEN +
					WLAN_STATUSCODE_LEN,
			     WLAN_AID_LEN);
		link_frame_currpos += WLAN_AID_LEN;
		link_frame_currlen += WLAN_AID_LEN;
		mlo_debug("Added AID field (%u octets) to link specific frame",
			  WLAN_AID_LEN);
	} else if (subtype == WLAN_FC0_STYPE_PROBE_RESP) {
		/* This is a probe response */
		mlo_debug("Populating fixed fields for probe response in link specific frame");

		if ((link_frame_maxsize - link_frame_currlen) <
				WLAN_TIMESTAMP_LEN) {
			mlo_err("Insufficient space in link specific frame for Timestamp Info field. Required: %u octets, available: %zu octets",
				WLAN_TIMESTAMP_LEN,
				(link_frame_maxsize - link_frame_currlen));

			ret = QDF_STATUS_E_NOMEM;
			goto mem_free;
		}

		/* Per spec 11be_D2.1.1, the TSF Offset subfield of the STA Info
		 * field indicates the offset (Toffset)between the TSF timer of
		 * the reported AP (TA) and the TSF timer of the reporting
		 * AP (TB) and is encoded as a 2s complement signed integer
		 * with units of 2 µs. Toffset is calculated as
		 * Toffset= Floor((TA – TB)/2).
		 */
		if (is_tsfoffset_valid)
			tsf += tsfoffset * 2;

		qdf_mem_copy(link_frame_currpos, &tsf, WLAN_TIMESTAMP_LEN);
		link_frame_currpos += WLAN_TIMESTAMP_LEN;
		link_frame_currlen += WLAN_TIMESTAMP_LEN;

		if (!is_beaconinterval_valid) {
			mlo_err_rl("Beacon interval information not present in STA info field of per-STA profile");
			ret = QDF_STATUS_E_PROTO;
			goto mem_free;
		}

		/* Beacon Interval information copy this from
		 * the STA info field.
		 */
		if ((link_frame_maxsize - link_frame_currlen) <
				WLAN_BEACONINTERVAL_LEN) {
			mlo_err("Insufficient space in link specific frame for Beacon Interval Info field. Required: %u octets, available: %zu octets",
				WLAN_BEACONINTERVAL_LEN,
				(link_frame_maxsize - link_frame_currlen));

			ret = QDF_STATUS_E_NOMEM;
			goto mem_free;
		}

		qdf_mem_copy(link_frame_currpos, &beaconinterval,
			     WLAN_BEACONINTERVAL_LEN);
		link_frame_currpos += WLAN_BEACONINTERVAL_LEN;
		link_frame_currlen += WLAN_BEACONINTERVAL_LEN;

		if (sta_prof_remlen < WLAN_CAPABILITYINFO_LEN) {
			mlo_err_rl("Remaining length of STA profile %zu octets is less than length of Capability Info %u",
				   sta_prof_remlen,
				   WLAN_CAPABILITYINFO_LEN);

			ret = QDF_STATUS_E_PROTO;
			goto mem_free;
		}

		/* Capability information is specific to the link. Copy this
		 * from the STA profile.
		 */

		if ((link_frame_maxsize - link_frame_currlen) <
				WLAN_CAPABILITYINFO_LEN) {
			mlo_err("Insufficient space in link specific frame for Capability Info field. Required: %u octets, available: %zu octets",
				WLAN_CAPABILITYINFO_LEN,
				(link_frame_maxsize - link_frame_currlen));

			ret = QDF_STATUS_E_NOMEM;
			goto mem_free;
		}

		qdf_mem_copy(link_frame_currpos, sta_prof_currpos,
			     WLAN_CAPABILITYINFO_LEN);
		link_frame_currpos += WLAN_CAPABILITYINFO_LEN;
		link_frame_currlen += WLAN_CAPABILITYINFO_LEN;

		sta_prof_currpos += WLAN_CAPABILITYINFO_LEN;
		sta_prof_remlen -= WLAN_CAPABILITYINFO_LEN;
	}

	sta_prof_iesection = sta_prof_currpos;
	sta_prof_iesection_len = sta_prof_remlen;

	/* Populate non-inheritance lists if applicable */
	ninherit_elemlist_len = 0;
	ninherit_elemlist = NULL;
	ninherit_elemextlist_len = 0;
	ninherit_elemextlist = NULL;

	ret = util_get_noninheritlists(sta_prof_iesection,
				       sta_prof_iesection_len,
				       &ninherit_elemlist,
				       &ninherit_elemlist_len,
				       &ninherit_elemextlist,
				       &ninherit_elemextlist_len);
	if (QDF_IS_STATUS_ERROR(ret))
		goto mem_free;

	/* Go through IEs of the reporting STA, and those in STA profile, merge
	 * them into link_frame (except for elements in the Non-Inheritance
	 * list).
	 *
	 * Note: Currently, only 2-link MLO is supported here. We may have a
	 * future change to expand to more links.
	 */
	reportingsta_ie = util_find_eid(WLAN_ELEMID_SSID, frame_iesection,
					frame_iesection_len);

	if ((subtype == WLAN_FC0_STYPE_ASSOC_REQ) ||
	    (subtype == WLAN_FC0_STYPE_REASSOC_REQ) ||
	    (subtype == WLAN_FC0_STYPE_PROBE_RESP)) {
		/* Sanity check that the SSID element is present for the
		 * reporting STA. There is no stipulation in the standard for
		 * the STA profile in this regard, so we do not check the STA
		 * profile for the SSID element.
		 */
		if (!reportingsta_ie) {
			mlo_err_rl("SSID element not found in reporting STA of the frame.");
			ret = QDF_STATUS_E_PROTO;
			goto mem_free;
		}
	} else {
		/* This is a (re)association response. Sanity check that the
		 * SSID element is present neither for the reporting STA nor in
		 * the STA profile.
		 */
		if (reportingsta_ie) {
			mlo_err_rl("SSID element found for reporting STA for (re)association response. It should not be present.");
			ret = QDF_STATUS_E_PROTO;
			goto mem_free;
		}

		sta_prof_ie = util_find_eid(WLAN_ELEMID_SSID,
					    sta_prof_iesection,
					    sta_prof_iesection_len);

		if (sta_prof_ie) {
			mlo_err_rl("SSID element found in STA profile for (re)association response. It should not be present.");
			ret = QDF_STATUS_E_PROTO;
			goto mem_free;
		}
	}

	reportingsta_ie = reportingsta_ie ? reportingsta_ie : frame_iesection;

	ret = util_validate_reportingsta_ie(reportingsta_ie, frame_iesection,
					    frame_iesection_len);
	if (QDF_IS_STATUS_ERROR(ret))
		goto mem_free;

	reportingsta_ie_size = reportingsta_ie[TAG_LEN_POS] + MIN_IE_LEN;

	while (((reportingsta_ie + reportingsta_ie_size) - frame_iesection)
			<= frame_iesection_len) {
		/* Skip Multi-Link element */
		if ((reportingsta_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) &&
		    (reportingsta_ie[IDEXT_POS] ==
				WLAN_EXTN_ELEMID_MULTI_LINK)) {
			if (((reportingsta_ie + reportingsta_ie_size) -
					frame_iesection) == frame_iesection_len)
				break;

			/* Add BV ML IE for link specific probe response */
			if (subtype == WLAN_FC0_STYPE_PROBE_RESP) {
				ret = util_add_mlie_for_prb_rsp_gen(
					reportingsta_ie,
					reportingsta_ie[TAG_LEN_POS],
					&link_frame_currpos,
					&link_frame_currlen,
					link_frame_maxsize,
					linkid);
				if (QDF_IS_STATUS_ERROR(ret))
					goto mem_free;
			}
			reportingsta_ie += reportingsta_ie_size;

			ret = util_validate_reportingsta_ie(reportingsta_ie,
							    frame_iesection,
							    frame_iesection_len);
			if (QDF_IS_STATUS_ERROR(ret))
				goto mem_free;

			reportingsta_ie_size = reportingsta_ie[TAG_LEN_POS] +
				MIN_IE_LEN;

			continue;
		}

		sta_prof_ie = NULL;
		sta_prof_ie_size = 0;

		if (sta_prof_iesection_len) {
			if (reportingsta_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) {
				sta_prof_ie = (uint8_t *)util_find_extn_eid(reportingsta_ie[ID_POS],
									    reportingsta_ie[IDEXT_POS],
									    sta_prof_iesection,
									    sta_prof_iesection_len);
			} else {
				sta_prof_ie = (uint8_t *)util_find_eid(reportingsta_ie[ID_POS],
								       sta_prof_iesection,
								       sta_prof_iesection_len);
			}
		}

		if (!sta_prof_ie) {
			/* IE is present for reporting STA, but not in STA
			 * profile.
			 */

			is_in_noninheritlist = false;

			ret = util_eval_ie_in_noninheritlist((uint8_t *)reportingsta_ie,
							     reportingsta_ie_size,
							     ninherit_elemlist,
							     ninherit_elemlist_len,
							     ninherit_elemextlist,
							     ninherit_elemextlist_len,
							     &is_in_noninheritlist);

			if (QDF_IS_STATUS_ERROR(ret))
				goto mem_free;

			if (!is_in_noninheritlist) {
				if ((link_frame_currpos +
						reportingsta_ie_size) <=
					(link_frame + link_frame_maxsize)) {
					qdf_mem_copy(link_frame_currpos,
						     reportingsta_ie,
						     reportingsta_ie_size);

					link_frame_currpos +=
						reportingsta_ie_size;
					link_frame_currlen +=
						reportingsta_ie_size;

					if (reportingsta_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) {
						mlo_etrace_debug("IE with element ID : %u extension element ID : %u (%zu octets) present for reporting STA but not in STA profile. Copied IE from reporting frame to link specific frame",
							      reportingsta_ie[ID_POS],
							      reportingsta_ie[IDEXT_POS],
							      reportingsta_ie_size);
					} else {
						mlo_etrace_debug("IE with element ID : %u (%zu octets) present for reporting STA but not in STA profile. Copied IE from reporting frame to link specific frame",
							      reportingsta_ie[ID_POS],
							      reportingsta_ie_size);
					}
				} else {
					if (reportingsta_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) {
						mlo_etrace_err_rl("Insufficient space in link specific frame for IE with element ID : %u extension element ID : %u. Required: %zu octets, available: %zu octets",
							       reportingsta_ie[ID_POS],
							       reportingsta_ie[IDEXT_POS],
							       reportingsta_ie_size,
							       link_frame_maxsize -
							       link_frame_currlen);
					} else {
						mlo_etrace_err_rl("Insufficient space in link specific frame for IE with element ID : %u. Required: %zu octets, available: %zu octets",
							       reportingsta_ie[ID_POS],
							       reportingsta_ie_size,
							       link_frame_maxsize -
							       link_frame_currlen);
					}

					ret = QDF_STATUS_E_NOMEM;
					goto mem_free;
				}
			} else {
				if (reportingsta_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) {
					mlo_etrace_debug("IE with element ID : %u extension element ID : %u (%zu octets) present for reporting STA but not in STA profile. However it is in Non-Inheritance list, hence ignoring.",
						      reportingsta_ie[ID_POS],
						      reportingsta_ie[IDEXT_POS],
						      reportingsta_ie_size);
				} else {
					mlo_etrace_debug("IE with element ID : %u (%zu octets) present for reporting STA but not in STA profile. However it is in Non-Inheritance list, hence ignoring.",
						      reportingsta_ie[ID_POS],
						      reportingsta_ie_size);
				}
			}
		} else {
			/* IE is present for reporting STA and also in STA
			 * profile, copy from STA profile and flag the IE in STA
			 * profile as copied (by setting EID field to 0). The
			 * SSID element (with EID 0) is processed first to
			 * enable this. For vendor IE, compare OUI + type +
			 * subType to determine if they are the same IE.
			 */
			/* Note: This may be revisited in a future change, to
			 * adhere to provisions in the standard for multiple
			 * occurrences of a given element ID/extension element
			 * ID.
			 */

			ret = util_validate_sta_prof_ie(sta_prof_ie,
							sta_prof_iesection,
							sta_prof_iesection_len);
			if (QDF_IS_STATUS_ERROR(ret))
				goto mem_free;

			sta_prof_ie_size = sta_prof_ie[TAG_LEN_POS] +
				MIN_IE_LEN;

			sta_prof_iesection_remlen =
				sta_prof_iesection_len -
					(sta_prof_ie - sta_prof_iesection);

			if ((reportingsta_ie[ID_POS] == WLAN_ELEMID_VENDOR) &&
			    (sta_prof_iesection_remlen >= MIN_VENDOR_TAG_LEN)) {
				/* If Vendor IE also presents in STA profile,
				 * then ignore the Vendor IE which is for
				 * reporting STA. It only needs to copy Vendor
				 * IE from STA profile to link specific frame.
				 * The copy happens when going through the
				 * remaining IEs.
				 */
				;
			} else {
				/* Copy IE from STA profile into link specific
				 * frame.
				 */
				if ((link_frame_currpos + sta_prof_ie_size) <=
					(link_frame + link_frame_maxsize)) {
					qdf_mem_copy(link_frame_currpos,
						     sta_prof_ie,
						     sta_prof_ie_size);

					link_frame_currpos += sta_prof_ie_size;
					link_frame_currlen +=
						sta_prof_ie_size;

					if (reportingsta_ie[ID_POS] ==
							WLAN_ELEMID_EXTN_ELEM) {
						mlo_etrace_debug("IE with element ID : %u extension element ID : %u (%zu octets) for reporting STA also present in STA profile. Copied IE from STA profile to link specific frame",
							      sta_prof_ie[ID_POS],
							      sta_prof_ie[IDEXT_POS],
							      sta_prof_ie_size);
					} else {
						mlo_etrace_debug("IE with element ID : %u (%zu octets) for reporting STA also present in STA profile. Copied IE from STA profile to link specific frame",
								 sta_prof_ie[ID_POS],
								 sta_prof_ie_size);
					}

					sta_prof_ie[0] = 0;
				} else {
					if (sta_prof_ie[ID_POS] ==
							WLAN_ELEMID_EXTN_ELEM) {
						mlo_etrace_err_rl("Insufficient space in link specific frame for IE with element ID : %u extension element ID : %u. Required: %zu octets, available: %zu octets",
								  sta_prof_ie[ID_POS],
								  sta_prof_ie[IDEXT_POS],
								  sta_prof_ie_size,
								  link_frame_maxsize -
								  link_frame_currlen);
					} else {
						mlo_etrace_err_rl("Insufficient space in link specific frame for IE with element ID : %u. Required: %zu octets, available: %zu octets",
								  sta_prof_ie[ID_POS],
								  sta_prof_ie_size,
								  link_frame_maxsize -
								  link_frame_currlen);
					}

					ret = QDF_STATUS_E_NOMEM;
					goto mem_free;
				}
			}
		}

		if (((reportingsta_ie + reportingsta_ie_size) -
					frame_iesection) == frame_iesection_len)
			break;

		reportingsta_ie += reportingsta_ie_size;

		ret = util_validate_reportingsta_ie(reportingsta_ie,
						    frame_iesection,
						    frame_iesection_len);
		if (QDF_IS_STATUS_ERROR(ret))
			goto mem_free;

		reportingsta_ie_size = reportingsta_ie[TAG_LEN_POS] +
			MIN_IE_LEN;
	}

	/* Go through the remaining unprocessed IEs in STA profile and copy them
	 * to the link specific frame. The processed ones are marked with 0 in
	 * the first octet. The first octet corresponds to the element ID. In
	 * the case of (re)association request, the element with actual ID
	 * WLAN_ELEMID_SSID(0) has already been copied to the link specific
	 * frame. In the case of (re)association response, it has been verified
	 * that the element with actual ID WLAN_ELEMID_SSID(0) is present
	 * neither for the reporting STA nor in the STA profile.
	 */
	sta_prof_iesection_currpos = sta_prof_iesection;
	sta_prof_iesection_remlen = sta_prof_iesection_len;

	while (sta_prof_iesection_remlen > 0) {
		sta_prof_ie = sta_prof_iesection_currpos;
		ret = util_validate_sta_prof_ie(sta_prof_ie,
						sta_prof_iesection_currpos,
						sta_prof_iesection_remlen);
		if (QDF_IS_STATUS_ERROR(ret))
			goto mem_free;

		sta_prof_ie_size = sta_prof_ie[TAG_LEN_POS] + MIN_IE_LEN;

		if (!sta_prof_ie[0]) {
			/* Skip this, since it has already been processed */
			sta_prof_iesection_currpos += sta_prof_ie_size;
			sta_prof_iesection_remlen -= sta_prof_ie_size;
			continue;
		}

		/* Copy IE from STA profile into link specific frame. */
		if ((link_frame_currpos + sta_prof_ie_size) <=
			(link_frame + link_frame_maxsize)) {
			qdf_mem_copy(link_frame_currpos,
				     sta_prof_ie,
				     sta_prof_ie_size);

			link_frame_currpos += sta_prof_ie_size;
			link_frame_currlen +=
				sta_prof_ie_size;

			if (reportingsta_ie[ID_POS] ==
					WLAN_ELEMID_EXTN_ELEM) {
				mlo_etrace_debug("IE with element ID : %u extension element ID : %u (%zu octets) is present only in STA profile. Copied IE from STA profile to link specific frame",
						 sta_prof_ie[ID_POS],
						 sta_prof_ie[IDEXT_POS],
						 sta_prof_ie_size);
			} else {
				mlo_etrace_debug("IE with element ID : %u (%zu octets) is present only in STA profile. Copied IE from STA profile to link specific frame",
						 sta_prof_ie[ID_POS],
						 sta_prof_ie_size);
			}

			sta_prof_ie[0] = 0;
		} else {
			if (sta_prof_ie[ID_POS] == WLAN_ELEMID_EXTN_ELEM) {
				mlo_etrace_err_rl("Insufficient space in link specific frame for IE with element ID : %u extension element ID : %u. Required: %zu octets, available: %zu octets",
						  sta_prof_ie[ID_POS],
						  sta_prof_ie[IDEXT_POS],
						  sta_prof_ie_size,
						  link_frame_maxsize -
						  link_frame_currlen);
			} else {
				mlo_etrace_err_rl("Insufficient space in link specific frame for IE with element ID : %u. Required: %zu octets, available: %zu octets",
						  sta_prof_ie[ID_POS],
						  sta_prof_ie_size,
						  link_frame_maxsize -
						  link_frame_currlen);
			}

			ret = QDF_STATUS_E_NOMEM;
			goto mem_free;
		}

		sta_prof_iesection_currpos += sta_prof_ie_size;
		sta_prof_iesection_remlen -= sta_prof_ie_size;
	}

	/* Copy the link MAC addr */
	link_frame_hdr = (struct wlan_frame_hdr *)link_frame;

	if ((subtype == WLAN_FC0_STYPE_ASSOC_REQ) ||
	    (subtype == WLAN_FC0_STYPE_REASSOC_REQ)) {
		qdf_mem_copy(link_frame_hdr->i_addr3, &link_addr,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(link_frame_hdr->i_addr2, reportedmacaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(link_frame_hdr->i_addr1, &link_addr,
			     QDF_MAC_ADDR_SIZE);

		link_frame_hdr->i_fc[0] = MLO_LINKSPECIFIC_ASSOC_REQ_FC0;
		link_frame_hdr->i_fc[1] = MLO_LINKSPECIFIC_ASSOC_REQ_FC1;
	} else if (subtype == WLAN_FC0_STYPE_PROBE_RESP) {
		qdf_mem_copy(link_frame_hdr->i_addr3, reportedmacaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(link_frame_hdr->i_addr2, reportedmacaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(link_frame_hdr->i_addr1, &link_addr,
			     QDF_MAC_ADDR_SIZE);

		link_frame_hdr->i_fc[0] = MLO_LINKSPECIFIC_PROBE_RESP_FC0;
		link_frame_hdr->i_fc[1] = MLO_LINKSPECIFIC_PROBE_RESP_FC1;
	} else {
		/* This is a (re)association response */

		qdf_mem_copy(link_frame_hdr->i_addr3, reportedmacaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(link_frame_hdr->i_addr2, reportedmacaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(link_frame_hdr->i_addr1, &link_addr,
			     QDF_MAC_ADDR_SIZE);

		link_frame_hdr->i_fc[0] = MLO_LINKSPECIFIC_ASSOC_RESP_FC0;
		link_frame_hdr->i_fc[1] = MLO_LINKSPECIFIC_ASSOC_RESP_FC1;
	}

	mlo_debug("subtype:%u addr3:" QDF_MAC_ADDR_FMT " addr2:"
		  QDF_MAC_ADDR_FMT " addr1:" QDF_MAC_ADDR_FMT,
		  subtype,
		  QDF_MAC_ADDR_REF(link_frame_hdr->i_addr3),
		  QDF_MAC_ADDR_REF(link_frame_hdr->i_addr2),
		  QDF_MAC_ADDR_REF(link_frame_hdr->i_addr1));

	/* Seq num not used so not populated */

	*link_frame_len = link_frame_currlen;
	ret = QDF_STATUS_SUCCESS;

mem_free:
	qdf_mem_free(mlieseqpayload_copy);
	return ret;
}

QDF_STATUS
util_gen_link_assoc_req(uint8_t *frame, qdf_size_t frame_len, bool isreassoc,
			uint8_t link_id,
			struct qdf_mac_addr link_addr,
			uint8_t *link_frame,
			qdf_size_t link_frame_maxsize,
			qdf_size_t *link_frame_len)
{
	return util_gen_link_reqrsp_cmn(frame, frame_len,
			(isreassoc ? WLAN_FC0_STYPE_REASSOC_REQ :
				WLAN_FC0_STYPE_ASSOC_REQ),
			link_id, link_addr, link_frame,
			link_frame_maxsize, link_frame_len);
}

QDF_STATUS
util_gen_link_assoc_rsp(uint8_t *frame, qdf_size_t frame_len, bool isreassoc,
			uint8_t link_id,
			struct qdf_mac_addr link_addr,
			uint8_t *link_frame,
			qdf_size_t link_frame_maxsize,
			qdf_size_t *link_frame_len)
{
	return util_gen_link_reqrsp_cmn(frame, frame_len,
			(isreassoc ?  WLAN_FC0_STYPE_REASSOC_RESP :
				WLAN_FC0_STYPE_ASSOC_RESP),
			link_id, link_addr, link_frame,
			link_frame_maxsize, link_frame_len);
}

QDF_STATUS
util_gen_link_probe_rsp(uint8_t *frame, qdf_size_t frame_len,
			uint8_t link_id,
			struct qdf_mac_addr link_addr,
			uint8_t *link_frame,
			qdf_size_t link_frame_maxsize,
			qdf_size_t *link_frame_len)
{
	return util_gen_link_reqrsp_cmn(frame, frame_len,
			 WLAN_FC0_STYPE_PROBE_RESP, link_id,
			link_addr, link_frame, link_frame_maxsize,
			link_frame_len);
}

QDF_STATUS
util_find_mlie(uint8_t *buf, qdf_size_t buflen, uint8_t **mlieseq,
	       qdf_size_t *mlieseqlen)
{
	uint8_t *bufboundary;
	uint8_t *ieseq;
	qdf_size_t ieseqlen;
	uint8_t *currie;
	uint8_t *successorfrag;

	if (!buf || !buflen || !mlieseq || !mlieseqlen)
		return QDF_STATUS_E_NULL_VALUE;

	*mlieseq = NULL;
	*mlieseqlen = 0;

	/* Find Multi-Link element. In case a fragment sequence is present,
	 * this element will be the leading fragment.
	 */
	ieseq = util_find_extn_eid(WLAN_ELEMID_EXTN_ELEM,
				   WLAN_EXTN_ELEMID_MULTI_LINK, buf,
				   buflen);

	/* Even if the element is not found, we have successfully examined the
	 * buffer. The caller will be provided a NULL value for the starting of
	 * the Multi-Link element. Hence, we return success.
	 */
	if (!ieseq)
		return QDF_STATUS_SUCCESS;

	bufboundary = buf + buflen;

	if ((ieseq + MIN_IE_LEN) > bufboundary)
		return QDF_STATUS_E_INVAL;

	ieseqlen = MIN_IE_LEN + ieseq[TAG_LEN_POS];

	if (ieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_PROTO;

	if ((ieseq + ieseqlen) > bufboundary)
		return QDF_STATUS_E_INVAL;

	/* In the next sequence of checks, if there is no space in the buffer
	 * for another element after the Multi-Link element/element fragment
	 * sequence, it could indicate an issue since non-MLO EHT elements
	 * would be expected to follow the Multi-Link element/element fragment
	 * sequence. However, this is outside of the purview of this function,
	 * hence we ignore it.
	 */

	currie = ieseq;
	successorfrag = util_get_successorfrag(currie, buf, buflen);

	/* Fragmentation definitions as of IEEE802.11be D1.0 and
	 * IEEE802.11REVme D0.2 are applied. Only the case where Multi-Link
	 * element is present in a buffer from the core frame is considered.
	 * Future changes to fragmentation, cases where the Multi-Link element
	 * is present in a subelement, etc. to be reflected here if applicable
	 * as and when the rules evolve.
	 */
	while (successorfrag) {
		/* We should not be seeing a successor fragment if the length
		 * of the current IE is lesser than the max.
		 */
		if (currie[TAG_LEN_POS] != WLAN_MAX_IE_LEN)
			return QDF_STATUS_E_PROTO;

		if (successorfrag[TAG_LEN_POS] == 0)
			return QDF_STATUS_E_PROTO;

		ieseqlen +=  (MIN_IE_LEN + successorfrag[TAG_LEN_POS]);

		currie = successorfrag;
		successorfrag = util_get_successorfrag(currie, buf, buflen);
	}

	*mlieseq = ieseq;
	*mlieseqlen = ieseqlen;
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
util_validate_bv_mlie_min_seq_len(qdf_size_t mlieseqlen)
{
	qdf_size_t parsed_len = sizeof(struct wlan_ie_multilink);

	if (mlieseqlen < parsed_len + WLAN_ML_BV_CINFO_LENGTH_SIZE) {
		mlo_err_rl("ML seq payload of len %zu doesn't accommodate the mandatory BV ML IE Common info len field",
			   mlieseqlen);
		return QDF_STATUS_E_PROTO;
	}
	parsed_len += WLAN_ML_BV_CINFO_LENGTH_SIZE;

	if (mlieseqlen < parsed_len + QDF_MAC_ADDR_SIZE) {
		mlo_err_rl("ML seq payload of len %zu doesn't accommodate the mandatory MLD addr",
			   mlieseqlen);
		return QDF_STATUS_E_PROTO;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_find_mlie_by_variant(uint8_t *buf, qdf_size_t buflen, uint8_t **mlieseq,
			  qdf_size_t *mlieseqlen, int variant)
{
	uint8_t *ieseq;
	qdf_size_t ieseqlen;
	QDF_STATUS status;
	int ml_variant;
	qdf_size_t buf_parsed_len;

	if (!buf || !buflen || !mlieseq || !mlieseqlen)
		return QDF_STATUS_E_NULL_VALUE;

	if (variant >= WLAN_ML_VARIANT_INVALIDSTART)
		return QDF_STATUS_E_PROTO;

	ieseq = NULL;
	ieseqlen = 0;
	*mlieseq = NULL;
	*mlieseqlen = 0;
	buf_parsed_len = 0;

	while (buflen > buf_parsed_len) {
		status = util_find_mlie(buf + buf_parsed_len,
					buflen - buf_parsed_len,
					&ieseq, &ieseqlen);

		if (QDF_IS_STATUS_ERROR(status))
			return status;

		/* Even if the element is not found, we have successfully
		 * examined the buffer. The caller will be provided a NULL value
		 * for the starting of the Multi-Link element. Hence, we return
		 * success.
		 */
		if (!ieseq)
			return QDF_STATUS_SUCCESS;

		status = util_get_mlie_variant(ieseq, ieseqlen,
					       &ml_variant);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("Unable to get Multi-link element variant");
			return status;
		}

		if (ml_variant == variant) {
			*mlieseq = ieseq;
			*mlieseqlen = ieseqlen;
			return QDF_STATUS_SUCCESS;
		}

		buf_parsed_len = ieseq + ieseqlen - buf;
	}

	return QDF_STATUS_E_INVAL;
}

QDF_STATUS
util_get_mlie_common_info_len(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			      uint8_t *commoninfo_len)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;

	if (!mlieseq || !mlieseqlen || !commoninfo_len)
		return QDF_STATUS_E_NULL_VALUE;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if (mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM ||
	    mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK)
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_INVAL;

	/* Common Info starts at mlieseq + sizeof(struct wlan_ie_multilink).
	 * Check if there is sufficient space in the buffer for the Common Info
	 * Length and MLD MAC address.
	 */
	if ((sizeof(struct wlan_ie_multilink) + WLAN_ML_BV_CINFO_LENGTH_SIZE +
	    QDF_MAC_ADDR_SIZE) > mlieseqlen)
		return QDF_STATUS_E_PROTO;

	*commoninfo_len = *(mlieseq + sizeof(struct wlan_ie_multilink));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_bssparamchangecnt(uint8_t *mlieseq, qdf_size_t mlieseqlen,
				  bool *bssparamchangecntfound,
				  uint8_t *bssparamchangecnt)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint16_t presencebitmap;
	uint8_t *commoninfo;
	uint8_t commoninfolen;
	qdf_size_t mldcap_offset;

	if (!mlieseq || !mlieseqlen || !bssparamchangecntfound ||
	    !bssparamchangecnt)
		return QDF_STATUS_E_NULL_VALUE;

	*bssparamchangecntfound = false;
	*bssparamchangecnt = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if (mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM ||
	    mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK)
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_NOSUPPORT;

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	if (QDF_IS_STATUS_ERROR(util_validate_bv_mlie_min_seq_len(mlieseqlen)))
		return QDF_STATUS_E_INVAL;

	commoninfo = mlieseq + sizeof(struct wlan_ie_multilink);
	commoninfolen = *(mlieseq + sizeof(struct wlan_ie_multilink));

	mldcap_offset = WLAN_ML_BV_CINFO_LENGTH_SIZE;

	mldcap_offset += QDF_MAC_ADDR_SIZE;

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P) {
		mldcap_offset += WLAN_ML_BV_CINFO_LINKIDINFO_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + mldcap_offset) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_BSSPARAMCHANGECNT_P) {
		if (commoninfolen < (mldcap_offset +
				     WLAN_ML_BSSPARAMCHNGCNT_SIZE))
			return QDF_STATUS_E_PROTO;

		if ((sizeof(struct wlan_ie_multilink) + mldcap_offset +
				WLAN_ML_BSSPARAMCHNGCNT_SIZE) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;
		*bssparamchangecntfound = true;
		*bssparamchangecnt = *(commoninfo + mldcap_offset);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_mlie_variant(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		      int *variant)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant var;
	uint16_t mlcontrol;

	if (!mlieseq || !mlieseqlen || !variant)
		return QDF_STATUS_E_NULL_VALUE;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK))
		return QDF_STATUS_E_INVAL;

	mlcontrol = le16toh(mlie_fixed->mlcontrol);
	var = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			   WLAN_ML_CTRL_TYPE_BITS);

	if (var >= WLAN_ML_VARIANT_INVALIDSTART)
		return QDF_STATUS_E_PROTO;

	*variant = var;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_eml_cap(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			bool *eml_cap_found,
			uint16_t *eml_cap)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint8_t eml_cap_offset;
	uint8_t commoninfo_len;
	uint16_t presencebitmap;

	if (!mlieseq || !mlieseqlen || !eml_cap_found || !eml_cap)
		return QDF_STATUS_E_NULL_VALUE;

	*eml_cap = 0;
	*eml_cap_found = false;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK))
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_INVAL;

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	/* eml_cap_offset stores the offset of EML Capabilities within
	 * Common Info
	 */
	eml_cap_offset = WLAN_ML_BV_CINFO_LENGTH_SIZE + QDF_MAC_ADDR_SIZE;
	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P)
		eml_cap_offset += WLAN_ML_BV_CINFO_LINKIDINFO_SIZE;
	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_BSSPARAMCHANGECNT_P)
		eml_cap_offset += WLAN_ML_BSSPARAMCHNGCNT_SIZE;
	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_MEDIUMSYNCDELAYINFO_P)
		eml_cap_offset += WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE;

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_EMLCAP_P) {
		/* Common Info starts at
		 * mlieseq + sizeof(struct wlan_ie_multilink).
		 * Check if there is sufficient space in the buffer for
		 * the Common Info Length.
		 */
		if (mlieseqlen < (sizeof(struct wlan_ie_multilink) +
				  WLAN_ML_BV_CINFO_LENGTH_SIZE))
			return QDF_STATUS_E_PROTO;

		/* Check if the value indicated in the Common Info Length
		 * subfield is sufficient to access the EML capabilities.
		 */
		commoninfo_len = *(mlieseq + sizeof(struct wlan_ie_multilink));
		if (commoninfo_len < (eml_cap_offset +
				      WLAN_ML_BV_CINFO_EMLCAP_SIZE))
			return QDF_STATUS_E_PROTO;

		/* Common Info starts at mlieseq + sizeof(struct
		 * wlan_ie_multilink). Check if there is sufficient space in
		 * Common Info for the EML capability.
		 */
		if (mlieseqlen < (sizeof(struct wlan_ie_multilink) +
				  eml_cap_offset +
				  WLAN_ML_BV_CINFO_EMLCAP_SIZE))
			return QDF_STATUS_E_PROTO;

		*eml_cap_found = true;
		*eml_cap = qdf_le16_to_cpu(*(uint16_t *)(mlieseq +
							 sizeof(struct wlan_ie_multilink) +
							 eml_cap_offset));
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_msd_cap(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			bool *msd_cap_found,
			uint16_t *msd_cap)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint8_t msd_cap_offset;
	uint8_t commoninfo_len;
	uint16_t presencebitmap;

	if (!mlieseq || !mlieseqlen || !msd_cap_found || !msd_cap)
		return QDF_STATUS_E_NULL_VALUE;

	*msd_cap = 0;
	*msd_cap_found = false;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK))
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_INVAL;

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	/* msd_cap_offset stores the offset of MSD capabilities within
	 * Common Info
	 */
	msd_cap_offset = WLAN_ML_BV_CINFO_LENGTH_SIZE + QDF_MAC_ADDR_SIZE;
	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P)
		msd_cap_offset += WLAN_ML_BV_CINFO_LINKIDINFO_SIZE;
	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_BSSPARAMCHANGECNT_P)
		msd_cap_offset += WLAN_ML_BSSPARAMCHNGCNT_SIZE;
	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_MEDIUMSYNCDELAYINFO_P) {
		/* Common Info starts at
		 * mlieseq + sizeof(struct wlan_ie_multilink).
		 * Check if there is sufficient space in the buffer for
		 * the Common Info Length.
		 */
		if (mlieseqlen < (sizeof(struct wlan_ie_multilink) +
				  WLAN_ML_BV_CINFO_LENGTH_SIZE))
			return QDF_STATUS_E_PROTO;

		/* Check if the value indicated in the Common Info Length
		 * subfield is sufficient to access the MSD capabilities.
		 */
		commoninfo_len = *(mlieseq + sizeof(struct wlan_ie_multilink));
		if (commoninfo_len < (msd_cap_offset +
				      WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE))
			return QDF_STATUS_E_PROTO;

		/* Common Info starts at mlieseq + sizeof(struct
		 * wlan_ie_multilink). Check if there is sufficient space in
		 * Common Info for the MSD capability.
		 */
		if (mlieseqlen < (sizeof(struct wlan_ie_multilink) +
				  msd_cap_offset +
				  WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE))
			return QDF_STATUS_E_PROTO;

		*msd_cap_found = true;
		*msd_cap = qdf_le16_to_cpu(*(uint16_t *)(mlieseq +
							 sizeof(struct wlan_ie_multilink) +
							 msd_cap_offset));
	} else {
		mlo_debug("MSD caps not found in assoc rsp");
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_mldmacaddr(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			   struct qdf_mac_addr *mldmacaddr)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint8_t commoninfo_len;

	if (!mlieseq || !mlieseqlen || !mldmacaddr)
		return QDF_STATUS_E_NULL_VALUE;

	qdf_mem_zero(mldmacaddr, sizeof(*mldmacaddr));

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK))
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_INVAL;

	/* Common Info starts at mlieseq + sizeof(struct wlan_ie_multilink).
	 * Check if there is sufficient space in the buffer for the Common Info
	 * Length and MLD MAC address.
	 */
	if ((sizeof(struct wlan_ie_multilink) + WLAN_ML_BV_CINFO_LENGTH_SIZE +
	    QDF_MAC_ADDR_SIZE) > mlieseqlen)
		return QDF_STATUS_E_PROTO;

	/* Check if the value indicated in the Common Info Length subfield is
	 * sufficient to access the MLD MAC address.
	 */
	commoninfo_len = *(mlieseq + sizeof(struct wlan_ie_multilink));
	if (commoninfo_len < (WLAN_ML_BV_CINFO_LENGTH_SIZE + QDF_MAC_ADDR_SIZE))
		return QDF_STATUS_E_PROTO;

	qdf_mem_copy(mldmacaddr->bytes,
		     mlieseq + sizeof(struct wlan_ie_multilink) +
		     WLAN_ML_BV_CINFO_LENGTH_SIZE,
		     QDF_MAC_ADDR_SIZE);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_primary_linkid(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			       bool *linkidfound, uint8_t *linkid)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint16_t presencebitmap;
	uint8_t *commoninfo;
	qdf_size_t commoninfolen;
	uint8_t *linkidinfo;

	if (!mlieseq || !mlieseqlen || !linkidfound || !linkid)
		return QDF_STATUS_E_NULL_VALUE;

	*linkidfound = false;
	*linkid = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK))
		return QDF_STATUS_E_INVAL;

	mlcontrol = le16toh(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_INVAL;

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	commoninfo = mlieseq + sizeof(struct wlan_ie_multilink);
	commoninfolen = 0;
	commoninfolen += WLAN_ML_BV_CINFO_LENGTH_SIZE;
	if ((sizeof(struct wlan_ie_multilink) + commoninfolen) >
			mlieseqlen)
		return QDF_STATUS_E_PROTO;

	commoninfolen += QDF_MAC_ADDR_SIZE;
	if ((sizeof(struct wlan_ie_multilink) + commoninfolen) >
			mlieseqlen)
		return QDF_STATUS_E_PROTO;

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P) {
		linkidinfo = commoninfo + commoninfolen;
		commoninfolen += WLAN_ML_BV_CINFO_LINKIDINFO_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + commoninfolen) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;

		*linkidfound = true;
		*linkid = QDF_GET_BITS(linkidinfo[0],
				       WLAN_ML_BV_CINFO_LINKIDINFO_LINKID_IDX,
				       WLAN_ML_BV_CINFO_LINKIDINFO_LINKID_BITS);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_mldcap(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		       bool *mldcapfound, uint16_t *mldcap)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint16_t presencebitmap;
	uint8_t *commoninfo;
	uint8_t commoninfo_len;
	qdf_size_t mldcap_offset;

	if (!mlieseq || !mlieseqlen || !mldcapfound || !mldcap)
		return QDF_STATUS_E_NULL_VALUE;

	*mldcapfound = false;
	*mldcap = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if (mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM ||
	    mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK)
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_NOSUPPORT;

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	if (QDF_IS_STATUS_ERROR(util_validate_bv_mlie_min_seq_len(mlieseqlen)))
		return QDF_STATUS_E_INVAL;

	commoninfo = mlieseq + sizeof(struct wlan_ie_multilink);
	commoninfo_len = *(mlieseq + sizeof(struct wlan_ie_multilink));
	/* mldcap_offset stores the offset of MLD Capabilities within
	 * Common Info
	 */
	mldcap_offset = WLAN_ML_BV_CINFO_LENGTH_SIZE;
	mldcap_offset += QDF_MAC_ADDR_SIZE;

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P) {
		mldcap_offset += WLAN_ML_BV_CINFO_LINKIDINFO_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + mldcap_offset) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_BSSPARAMCHANGECNT_P) {
		mldcap_offset += WLAN_ML_BSSPARAMCHNGCNT_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + mldcap_offset) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_MEDIUMSYNCDELAYINFO_P) {
		mldcap_offset += WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + mldcap_offset) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_EMLCAP_P) {
		mldcap_offset += WLAN_ML_BV_CINFO_EMLCAP_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + mldcap_offset) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presencebitmap & WLAN_ML_BV_CTRL_PBM_MLDCAPANDOP_P) {
		/* Check if the value indicated in the Common Info Length
		 * subfield is sufficient to access the MLD capabilities.
		 */
		if (commoninfo_len < (mldcap_offset +
				      WLAN_ML_BV_CINFO_MLDCAPANDOP_SIZE))
			return QDF_STATUS_E_PROTO;

		if ((sizeof(struct wlan_ie_multilink) + mldcap_offset +
					WLAN_ML_BV_CINFO_MLDCAPANDOP_SIZE) >
				mlieseqlen)
			return QDF_STATUS_E_PROTO;

		*mldcap = qdf_le16_to_cpu(*((uint16_t *)(commoninfo + mldcap_offset)));
		*mldcapfound = true;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_ext_mld_cap_op_info(uint8_t *mlie_seq,
				    qdf_size_t mlie_seqlen,
				    bool *ext_mld_cap_found,
				    uint16_t *ext_mld_cap)
{
	struct wlan_ie_multilink *mlie_fixed;
	uint16_t mlcontrol;
	enum wlan_ml_variant variant;
	uint16_t presence_bitmap;
	uint8_t *commoninfo;
	uint8_t commoninfo_len;
	qdf_size_t extmldcap_offset;

	if (!mlie_seq || !mlie_seqlen || !ext_mld_cap_found || !ext_mld_cap)
		return QDF_STATUS_E_NULL_VALUE;

	*ext_mld_cap_found = false;
	*ext_mld_cap = 0;

	if (mlie_seqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlie_seq;

	if (mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM ||
	    mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK)
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC)
		return QDF_STATUS_E_NOSUPPORT;

	presence_bitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				       WLAN_ML_CTRL_PBM_BITS);

	commoninfo = mlie_seq + sizeof(struct wlan_ie_multilink);
	commoninfo_len = *(mlie_seq + sizeof(struct wlan_ie_multilink));
	/* extmldcap_offset stores the offset of Ext MLD Capabilities and
	 * operations within the Common Info
	 */

	extmldcap_offset = WLAN_ML_BV_CINFO_LENGTH_SIZE;
	extmldcap_offset += QDF_MAC_ADDR_SIZE;

	if (presence_bitmap & WLAN_ML_BV_CTRL_PBM_LINKIDINFO_P) {
		extmldcap_offset += WLAN_ML_BV_CINFO_LINKIDINFO_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + extmldcap_offset) >
				mlie_seqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presence_bitmap & WLAN_ML_BV_CTRL_PBM_BSSPARAMCHANGECNT_P) {
		extmldcap_offset += WLAN_ML_BSSPARAMCHNGCNT_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + extmldcap_offset) >
				mlie_seqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presence_bitmap & WLAN_ML_BV_CTRL_PBM_MEDIUMSYNCDELAYINFO_P) {
		extmldcap_offset += WLAN_ML_BV_CINFO_MEDMSYNCDELAYINFO_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + extmldcap_offset) >
				mlie_seqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presence_bitmap & WLAN_ML_BV_CTRL_PBM_EMLCAP_P) {
		extmldcap_offset += WLAN_ML_BV_CINFO_EMLCAP_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + extmldcap_offset) >
				mlie_seqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presence_bitmap & WLAN_ML_BV_CTRL_PBM_MLDCAPANDOP_P) {
		extmldcap_offset += WLAN_ML_BV_CINFO_MLDCAPANDOP_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + extmldcap_offset) >
				mlie_seqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presence_bitmap & WLAN_ML_BV_CTRL_PBM_MLDID_P) {
		extmldcap_offset += WLAN_ML_BV_CINFO_MLDID_SIZE;

		if ((sizeof(struct wlan_ie_multilink) + extmldcap_offset) >
				mlie_seqlen)
			return QDF_STATUS_E_PROTO;
	}

	if (presence_bitmap & WLAN_ML_BV_CTRL_PBM_EXT_MLDCAPANDOP_P) {
		/* Check if the value indicated in the Common Info Length
		 * subfield is sufficient to access the Ext MLD capabilities.
		 */
		if (commoninfo_len < (extmldcap_offset +
					WLAN_ML_BV_CINFO_EXT_MLDCAPANDOP_SIZE))
			return QDF_STATUS_E_PROTO;

		if ((sizeof(struct wlan_ie_multilink) + extmldcap_offset +
					WLAN_ML_BV_CINFO_EXT_MLDCAPANDOP_SIZE) >
				mlie_seqlen)
			return QDF_STATUS_E_PROTO;

		*ext_mld_cap = qdf_le16_to_cpu(*((uint16_t *)(commoninfo +
						extmldcap_offset)));

		*ext_mld_cap_found = true;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_bvmlie_persta_partner_info(uint8_t *mlieseq,
				    qdf_size_t mlieseqlen,
				    struct mlo_partner_info *partner_info)
{
	struct wlan_ie_multilink *mlie_fixed;
	uint16_t mlcontrol;
	enum wlan_ml_variant variant;
	uint8_t *linkinfo;
	qdf_size_t linkinfo_len;
	struct mlo_partner_info pinfo = {0};
	qdf_size_t mlieseqpayloadlen;
	uint8_t *mlieseqpayload_copy;
	bool is_elemfragseq;
	qdf_size_t defragpayload_len;

	qdf_size_t tmplen;
	QDF_STATUS ret;

	if (!mlieseq) {
		mlo_err("Pointer to Multi-Link element sequence is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlieseqlen) {
		mlo_err("Length of Multi-Link element sequence is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (!partner_info) {
		mlo_err("partner_info is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	partner_info->num_partner_links = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink)) {
		mlo_err_rl("Multi-Link element sequence length %zu octets is smaller than required for the fixed portion of Multi-Link element (%zu octets)",
			   mlieseqlen, sizeof(struct wlan_ie_multilink));
		return QDF_STATUS_E_INVAL;
	}

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK)) {
		mlo_err("The element is not a Multi-Link element");
		return QDF_STATUS_E_INVAL;
	}

	mlcontrol = le16toh(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_BASIC) {
		mlo_err("The variant value %u does not correspond to Basic Variant value %u",
			variant, WLAN_ML_VARIANT_BASIC);
		return QDF_STATUS_E_INVAL;
	}

	mlieseqpayloadlen = 0;
	tmplen = 0;
	is_elemfragseq = false;

	ret = wlan_get_elem_fragseq_info(mlieseq,
					 mlieseqlen,
					 &is_elemfragseq,
					 &tmplen,
					 &mlieseqpayloadlen);
	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	if (is_elemfragseq) {
		if (tmplen != mlieseqlen) {
			mlo_err_rl("Mismatch in values of element fragment sequence total length. Val per frag info determination: %zu octets, val passed as arg: %zu octets",
				   tmplen, mlieseqlen);
			return QDF_STATUS_E_INVAL;
		}

		if (!mlieseqpayloadlen) {
			mlo_err_rl("Multi-Link element fragment sequence payload is reported as 0, investigate");
			return QDF_STATUS_E_FAILURE;
		}

		mlo_debug("Multi-Link element fragment sequence found with payload len %zu",
			  mlieseqpayloadlen);
	} else {
		if (mlieseqlen > (sizeof(struct ie_header) + WLAN_MAX_IE_LEN)) {
			mlo_err_rl("Expected presence of valid fragment sequence since Multi-Link element sequence length %zu octets is larger than frag threshold of %zu octets, however no valid fragment sequence found",
				   mlieseqlen,
				   sizeof(struct ie_header) + WLAN_MAX_IE_LEN);
			return QDF_STATUS_E_FAILURE;
		}

		mlieseqpayloadlen = mlieseqlen - (sizeof(struct ie_header) + 1);
	}

	mlieseqpayload_copy = qdf_mem_malloc(mlieseqpayloadlen);

	if (!mlieseqpayload_copy) {
		mlo_err_rl("Could not allocate memory for Multi-Link element payload copy");
		return QDF_STATUS_E_NOMEM;
	}

	if (is_elemfragseq) {
		ret = wlan_defrag_elem_fragseq(false,
					       mlieseq,
					       mlieseqlen,
					       mlieseqpayload_copy,
					       mlieseqpayloadlen,
					       &defragpayload_len);
		if (QDF_IS_STATUS_ERROR(ret)) {
			qdf_mem_free(mlieseqpayload_copy);
			return ret;
		}

		if (defragpayload_len != mlieseqpayloadlen) {
			mlo_err_rl("Length of de-fragmented payload %zu octets is not equal to length of Multi-Link element fragment sequence payload %zu octets",
				   defragpayload_len, mlieseqpayloadlen);
			qdf_mem_free(mlieseqpayload_copy);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		qdf_mem_copy(mlieseqpayload_copy,
			     mlieseq + sizeof(struct ie_header) + 1,
			     mlieseqpayloadlen);
	}

	linkinfo = NULL;
	linkinfo_len = 0;

	ret = util_parse_multi_link_ctrl(mlieseqpayload_copy,
					 mlieseqpayloadlen,
					 &linkinfo,
					 &linkinfo_len);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(mlieseqpayload_copy);
		return ret;
	}

	/*
	 * If Probe Request variant Multi-Link element in the Multi-Link probe
	 * request does not include any per-STA profile, then all APs affiliated
	 * with the same AP MLD as the AP identified in the Addr 1 or Addr 3
	 * field or AP MLD ID of the Multi-Link probe request are requested
	 * APs return success here
	 */
	if (!linkinfo) {
		qdf_mem_free(mlieseqpayload_copy);
		return QDF_STATUS_SUCCESS;
	}

	ret = util_parse_partner_info_from_linkinfo(linkinfo,
						    linkinfo_len,
						    &pinfo);

	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(mlieseqpayload_copy);
		return ret;
	}

	qdf_mem_copy(partner_info, &pinfo, sizeof(*partner_info));

	qdf_mem_free(mlieseqpayload_copy);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_prvmlie_persta_link_id(uint8_t *mlieseq,
				qdf_size_t mlieseqlen,
				struct mlo_probereq_info *probereq_info)
{
	struct wlan_ie_multilink *mlie_fixed;
	uint16_t mlcontrol;
	enum wlan_ml_variant variant;
	uint8_t *linkinfo;
	qdf_size_t linkinfo_len;
	qdf_size_t mlieseqpayloadlen;
	uint8_t *mlieseqpayload_copy;
	bool is_elemfragseq;
	qdf_size_t defragpayload_len;

	qdf_size_t tmplen;
	QDF_STATUS ret;

	if (!mlieseq) {
		mlo_err("Pointer to Multi-Link element sequence is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlieseqlen) {
		mlo_err("Length of Multi-Link element sequence is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (!probereq_info) {
		mlo_err("probe request_info is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	probereq_info->num_links = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink)) {
		mlo_err_rl("Multi-Link element sequence length %zu octets is smaller than required for the fixed portion of Multi-Link element (%zu octets)",
			   mlieseqlen, sizeof(struct wlan_ie_multilink));
		return QDF_STATUS_E_INVAL;
	}

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if ((mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM) ||
	    (mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK)) {
		mlo_err("The element is not a Multi-Link element");
		return QDF_STATUS_E_INVAL;
	}

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_PROBEREQ) {
		mlo_err("The variant value %u does not correspond to Probe Request Variant value %u",
			variant, WLAN_ML_VARIANT_PROBEREQ);
		return QDF_STATUS_E_INVAL;
	}

	mlieseqpayloadlen = 0;
	tmplen = 0;
	is_elemfragseq = false;

	ret = wlan_get_elem_fragseq_info(mlieseq,
					 mlieseqlen,
					 &is_elemfragseq,
					 &tmplen,
					 &mlieseqpayloadlen);
	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	if (is_elemfragseq) {
		if (tmplen != mlieseqlen) {
			mlo_err_rl("Mismatch in values of element fragment sequence total length. Val per frag info determination: %zu octets, val passed as arg: %zu octets",
				   tmplen, mlieseqlen);
			return QDF_STATUS_E_INVAL;
		}

		if (!mlieseqpayloadlen) {
			mlo_err_rl("Multi-Link element fragment sequence payload is reported as 0, investigate");
			return QDF_STATUS_E_FAILURE;
		}

		mlo_debug("Multi-Link element fragment sequence found with payload len %zu",
			  mlieseqpayloadlen);
	} else {
		if (mlieseqlen > (sizeof(struct ie_header) + WLAN_MAX_IE_LEN)) {
			mlo_err_rl("Expected presence of valid fragment sequence since Multi-Link element sequence length %zu octets is larger than frag threshold of %zu octets, however no valid fragment sequence found",
				   mlieseqlen,
				   sizeof(struct ie_header) + WLAN_MAX_IE_LEN);
			return QDF_STATUS_E_FAILURE;
		}

		mlieseqpayloadlen = mlieseqlen - (sizeof(struct ie_header) + 1);
	}

	mlieseqpayload_copy = qdf_mem_malloc(mlieseqpayloadlen);

	if (!mlieseqpayload_copy) {
		mlo_err_rl("Could not allocate memory for Multi-Link element payload copy");
		return QDF_STATUS_E_NOMEM;
	}

	if (is_elemfragseq) {
		ret = wlan_defrag_elem_fragseq(false,
					       mlieseq,
					       mlieseqlen,
					       mlieseqpayload_copy,
					       mlieseqpayloadlen,
					       &defragpayload_len);
		if (QDF_IS_STATUS_ERROR(ret)) {
			qdf_mem_free(mlieseqpayload_copy);
			return ret;
		}

		if (defragpayload_len != mlieseqpayloadlen) {
			mlo_err_rl("Length of de-fragmented payload %zu octets is not equal to length of Multi-Link element fragment sequence payload %zu octets",
				   defragpayload_len, mlieseqpayloadlen);
			qdf_mem_free(mlieseqpayload_copy);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		qdf_mem_copy(mlieseqpayload_copy,
			     mlieseq + sizeof(struct ie_header) + 1,
			     mlieseqpayloadlen);
	}

	linkinfo = NULL;
	linkinfo_len = 0;
	ret = util_parse_prv_multi_link_ctrl(mlieseqpayload_copy,
					     mlieseqpayloadlen,
					     &linkinfo,
					     &linkinfo_len);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(mlieseqpayload_copy);
		return ret;
	}

	/* In case Link Info is absent, the number of links will remain
	 * zero.
	 */
	if (!linkinfo) {
		mlo_debug("No link info present");
		qdf_mem_free(mlieseqpayload_copy);
		return QDF_STATUS_SUCCESS;
	}

	ret = util_parse_probereq_info_from_linkinfo(linkinfo,
						     linkinfo_len,
						     probereq_info);

	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(mlieseqpayload_copy);
		return ret;
	}

	qdf_mem_free(mlieseqpayload_copy);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_prvmlie_mldid(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		       bool *mldidfound, uint8_t *mldid)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint16_t presencebitmap;
	uint8_t *commoninfo;
	qdf_size_t commoninfolen;

	if (!mlieseq || !mlieseqlen || !mldidfound || !mldid)
		return QDF_STATUS_E_NULL_VALUE;

	*mldidfound = false;
	*mldid = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if (mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM ||
	    mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK)
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_PROBEREQ)
		return QDF_STATUS_E_NOSUPPORT;

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	commoninfo = mlieseq + sizeof(struct wlan_ie_multilink);
	commoninfolen = WLAN_ML_PRV_CINFO_LENGTH_SIZE;

	if (presencebitmap & WLAN_ML_PRV_CTRL_PBM_MLDID_P) {
		if ((sizeof(struct wlan_ie_multilink) + commoninfolen +
		     WLAN_ML_PRV_CINFO_MLDID_SIZE) >
		    mlieseqlen)
			return QDF_STATUS_E_PROTO;

		*mldid = *((uint8_t *)(commoninfo + commoninfolen));
		commoninfolen += WLAN_ML_PRV_CINFO_MLDID_SIZE;

		*mldidfound = true;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS util_get_rvmlie_mldmacaddr(uint8_t *mlieseq, qdf_size_t mlieseqlen,
				      struct qdf_mac_addr *mldmacaddr,
				      bool *is_mldmacaddr_found)
{
	struct wlan_ie_multilink *mlie_fixed;
	enum wlan_ml_variant variant;
	uint16_t mlcontrol;
	uint16_t presencebitmap;
	qdf_size_t rv_cinfo_len;

	if (!mlieseq || !mlieseqlen || !mldmacaddr || !is_mldmacaddr_found)
		return QDF_STATUS_E_NULL_VALUE;

	*is_mldmacaddr_found = false;
	qdf_mem_zero(mldmacaddr, sizeof(*mldmacaddr));

	if (mlieseqlen < sizeof(struct wlan_ie_multilink))
		return QDF_STATUS_E_INVAL;

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;

	if (mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM ||
	    mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK)
		return QDF_STATUS_E_INVAL;

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_RECONFIG)
		return QDF_STATUS_E_INVAL;

	/* ML Reconfig Common Info Length field present */
	if ((sizeof(struct wlan_ie_multilink) + WLAN_ML_RV_CINFO_LENGTH_SIZE) >
	    mlieseqlen)
		return QDF_STATUS_E_PROTO;

	rv_cinfo_len = *(mlieseq + sizeof(struct wlan_ie_multilink));

	presencebitmap = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				      WLAN_ML_CTRL_PBM_BITS);

	/* Check if MLD mac address is present */
	if (presencebitmap & WLAN_ML_RV_CTRL_PBM_MLDMACADDR_P) {
		/* Check if the value indicated in the Common Info Length
		 * subfield is sufficient to access the MLD MAC address.
		 */
		if (rv_cinfo_len < (WLAN_ML_RV_CINFO_LENGTH_SIZE +
				    QDF_MAC_ADDR_SIZE))
			return QDF_STATUS_E_PROTO;

		if ((sizeof(struct wlan_ie_multilink) +
		     WLAN_ML_RV_CINFO_LENGTH_SIZE + QDF_MAC_ADDR_SIZE) >
			mlieseqlen)
			return QDF_STATUS_E_PROTO;

		qdf_mem_copy(mldmacaddr->bytes,
			     mlieseq + sizeof(struct wlan_ie_multilink) +
			     WLAN_ML_RV_CINFO_LENGTH_SIZE,
			     QDF_MAC_ADDR_SIZE);
		*is_mldmacaddr_found = true;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_rv_multi_link_ctrl(uint8_t *mlieseqpayload,
			      qdf_size_t mlieseqpayloadlen,
			      uint8_t **link_info,
			      qdf_size_t *link_info_len)
{
	qdf_size_t parsed_payload_len, rv_cinfo_len;
	uint16_t mlcontrol;
	uint16_t presence_bm;

	/* This helper returns the location(s) and length(s) of (sub)field(s)
	 * inferable after parsing the Multi Link element Control field. These
	 * location(s) and length(s) is/are in reference to the payload section
	 * of the Multi Link element (after defragmentation, if applicable).
	 * Here, the payload is the point after the element ID extension of the
	 * Multi Link element, and includes the payloads of all subsequent
	 * fragments (if any) but not the headers of those fragments.
	 *
	 * Currently, the helper returns the location and length of the Link
	 * Info field in the Multi Link element sequence. Other (sub)field(s)
	 * can be added later as required.
	 */
	if (!mlieseqpayload) {
		mlo_err("ML seq payload pointer is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlieseqpayloadlen) {
		mlo_err("ML seq payload len is 0");
		return QDF_STATUS_E_INVAL;
	}

	if (mlieseqpayloadlen < WLAN_ML_CTRL_SIZE) {
		mlo_err_rl("ML seq payload len %zu < ML Control size %u",
			   mlieseqpayloadlen, WLAN_ML_CTRL_SIZE);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len = 0;

	qdf_mem_copy(&mlcontrol, mlieseqpayload, WLAN_ML_CTRL_SIZE);
	mlcontrol = qdf_le16_to_cpu(mlcontrol);
	parsed_payload_len += WLAN_ML_CTRL_SIZE;

	if (mlieseqpayloadlen <
			(parsed_payload_len + WLAN_ML_RV_CINFO_LENGTH_SIZE)) {
		mlo_err_rl("ML rv seq payload len %zu insufficient for common info length size %u after parsed payload len %zu.",
			   mlieseqpayloadlen,
			   WLAN_ML_RV_CINFO_LENGTH_SIZE,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	rv_cinfo_len = *(mlieseqpayload + parsed_payload_len);
	parsed_payload_len += WLAN_ML_RV_CINFO_LENGTH_SIZE;

	presence_bm = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_PBM_IDX,
				   WLAN_ML_CTRL_PBM_BITS);

	/* Check if MLD MAC address is present */
	if (presence_bm & WLAN_ML_RV_CTRL_PBM_MLDMACADDR_P) {
		/* Check if the value indicated in the Common Info Length
		 * subfield is sufficient to access the MLD MAC address.
		 * Note: In D3.0, MLD MAC address will not be present
		 * in ML Reconfig IE. But for code completeness, we
		 * should have below code to sanity check.
		 */
		if (rv_cinfo_len < (WLAN_ML_RV_CINFO_LENGTH_SIZE +
				    QDF_MAC_ADDR_SIZE)) {
			mlo_err_rl("ML rv Common Info Length %zu insufficient to access MLD MAC addr size %u.",
				   rv_cinfo_len,
				   QDF_MAC_ADDR_SIZE);
			return QDF_STATUS_E_PROTO;
		}

		if (mlieseqpayloadlen <
				(parsed_payload_len +
				 QDF_MAC_ADDR_SIZE)) {
			mlo_err_rl("ML seq payload len %zu insufficient for MLD MAC size %u after parsed payload len %zu.",
				   mlieseqpayloadlen,
				   QDF_MAC_ADDR_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		parsed_payload_len += QDF_MAC_ADDR_SIZE;
	}

	/* At present, we only handle MAC address field in common info field.
	 * To be compatible with future spec updating, if new items are added
	 * to common info, below log will highlight the spec change.
	 */
	if (rv_cinfo_len != (parsed_payload_len - WLAN_ML_CTRL_SIZE))
		mlo_debug_rl("ML rv seq common info len %zu doesn't match with expected common info len %zu",
			     rv_cinfo_len,
			     parsed_payload_len - WLAN_ML_CTRL_SIZE);

	if (mlieseqpayloadlen < (WLAN_ML_CTRL_SIZE + rv_cinfo_len)) {
		mlo_err_rl("ML seq payload len %zu insufficient for rv link info after parsed mutli-link control %u and indicated Common Info length %zu",
			   mlieseqpayloadlen,
			   WLAN_ML_CTRL_SIZE,
			   rv_cinfo_len);
		return QDF_STATUS_E_PROTO;
	}
	/* Update parsed_payload_len to reflect the actual bytes in common info
	 * field to be compatible with future spec updating.
	 */
	parsed_payload_len = WLAN_ML_CTRL_SIZE + rv_cinfo_len;

	if (link_info_len) {
		*link_info_len = mlieseqpayloadlen - parsed_payload_len;
		mlo_debug("link_info_len:%zu, parsed_payload_len:%zu, rv_cinfo_len %zu ",
			  *link_info_len, parsed_payload_len, rv_cinfo_len);
	}

	if (mlieseqpayloadlen == parsed_payload_len) {
		mlo_debug("No Link Info field present");
		if (link_info)
			*link_info = NULL;

		return QDF_STATUS_SUCCESS;
	}

	if (link_info)
		*link_info = mlieseqpayload + parsed_payload_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_rvmlie_perstaprofile_stactrl(uint8_t *subelempayload,
					qdf_size_t subelempayloadlen,
					uint8_t *linkid,
					bool *is_macaddr_valid,
					struct qdf_mac_addr *macaddr,
					bool *is_ap_removal_timer_valid,
					uint16_t *ap_removal_timer)
{
	qdf_size_t parsed_payload_len = 0, sta_info_len;
	qdf_size_t parsed_sta_info_len;
	uint16_t stacontrol;
	uint8_t completeprofile;

	/* This helper returns the location(s) and where required, the length(s)
	 * of (sub)field(s) inferable after parsing the STA Control field in the
	 * per-STA profile subelement. These location(s) and length(s) is/are in
	 * reference to the payload section of the per-STA profile subelement
	 * (after defragmentation, if applicable).  Here, the payload is the
	 * point after the subelement length in the subelement, and includes the
	 * payloads of all subsequent fragments (if any) but not the headers of
	 * those fragments.
	 *
	 * Currently, the helper returns the link ID, MAC address, AP removal
	 * timer and STA profile. More (sub)fields can be added when required.
	 */
	if (!subelempayload) {
		mlo_err("Pointer to subelement payload is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!subelempayloadlen) {
		mlo_err("Length of subelement payload is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (subelempayloadlen < WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_SIZE) {
		mlo_err_rl("Subelement payload length %zu octets is smaller than STA control field of per-STA profile subelement %u octets",
			   subelempayloadlen,
			   WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_SIZE);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len = 0;
	qdf_mem_copy(&stacontrol,
		     subelempayload,
		     WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_SIZE);

	stacontrol = qdf_le16_to_cpu(stacontrol);
	parsed_payload_len += WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_SIZE;

	if (linkid)
		*linkid = QDF_GET_BITS(stacontrol,
				WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_LINKID_IDX,
				WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_LINKID_BITS);

	/* Check if this a complete profile */
	completeprofile = QDF_GET_BITS(stacontrol,
				WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_IDX,
				WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_CMPLTPROF_BITS);

	if (is_macaddr_valid)
		*is_macaddr_valid = false;
	if (subelempayloadlen < parsed_payload_len +
		WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE) {
		mlo_err_rl("Length of subelement payload %zu octets not sufficient for sta info length of size %u octets after parsed payload length of %zu octets.",
			   subelempayloadlen, WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	sta_info_len = *(subelempayload + parsed_payload_len);
	parsed_payload_len += WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE;
	parsed_sta_info_len = WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_LENGTH_SIZE;

	/* Check STA MAC address present bit */
	if (QDF_GET_BITS(stacontrol,
			 WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_STAMACADDRP_IDX,
			 WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_STAMACADDRP_BITS)) {
		if (sta_info_len < (parsed_sta_info_len + QDF_MAC_ADDR_SIZE)) {
			mlo_err_rl("Length of sta info len %zu octets not sufficient to contain MAC address of size %u octets after parsed sta info length of %zu octets.",
				   sta_info_len, QDF_MAC_ADDR_SIZE,
				   parsed_sta_info_len);
			return QDF_STATUS_E_PROTO;
		}

		if (subelempayloadlen <
				(parsed_payload_len + QDF_MAC_ADDR_SIZE)) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain MAC address of size %u octets after parsed payload length of %zu octets.",
				   subelempayloadlen, QDF_MAC_ADDR_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		if (macaddr) {
			qdf_mem_copy(macaddr->bytes,
				     subelempayload + parsed_payload_len,
				     QDF_MAC_ADDR_SIZE);
			mlo_nofl_debug("Copied MAC address: " QDF_MAC_ADDR_FMT,
				       QDF_MAC_ADDR_REF(macaddr->bytes));

			if (is_macaddr_valid)
				*is_macaddr_valid = true;
		}

		parsed_payload_len += QDF_MAC_ADDR_SIZE;
		parsed_sta_info_len += QDF_MAC_ADDR_SIZE;
	}

	/* Check AP Removal timer present bit */
	if (QDF_GET_BITS(stacontrol,
			 WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_APREMOVALTIMERP_IDX,
			 WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_APREMOVALTIMERP_BITS)) {
		if (sta_info_len <
				(parsed_sta_info_len +
				 WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE)) {
			mlo_err_rl("Length of sta info len %zu octets not sufficient to contain AP removal timer of size %u octets after parsed sta info length of %zu octets.",
				   sta_info_len, WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE,
				   parsed_sta_info_len);
			return QDF_STATUS_E_PROTO;
		}

		if (subelempayloadlen <
				(parsed_payload_len +
				 WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE)) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain AP removal timer of size %u octets after parsed payload length of %zu octets.",
				   subelempayloadlen,
				   WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		if (ap_removal_timer) {
			qdf_mem_copy(ap_removal_timer,
				     subelempayload + parsed_payload_len,
				     WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE);

			if (is_ap_removal_timer_valid)
				*is_ap_removal_timer_valid = true;
		}

		parsed_payload_len +=
			WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE;
		parsed_sta_info_len += WLAN_ML_RV_LINFO_PERSTAPROF_STAINFO_APREMOVALTIMER_SIZE;
	}
	/* At present, we only handle link MAC address field and ap removal
	 * timer tbtt field parsing. To be compatible with future spec
	 * updating, if new items are added to sta info, below log will
	 * highlight the spec change.
	 */
	if (sta_info_len != (parsed_payload_len -
			     WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_SIZE))
		mlo_debug_rl("Length of sta info len %zu octets not match parsed payload length of %zu octets.",
			     sta_info_len,
			     parsed_payload_len -
			     WLAN_ML_RV_LINFO_PERSTAPROF_STACTRL_SIZE);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_rv_info_from_linkinfo(uint8_t *linkinfo,
				 qdf_size_t linkinfo_len,
				 struct ml_rv_info *reconfig_info)
{
	uint8_t linkid;
	uint8_t *linkinfo_currpos;
	qdf_size_t linkinfo_remlen;
	bool is_subelemfragseq;
	uint8_t subelemid;
	qdf_size_t subelemseqtotallen;
	qdf_size_t subelemseqpayloadlen;
	qdf_size_t defragpayload_len;
	QDF_STATUS ret;
	struct qdf_mac_addr mac_addr;
	bool is_macaddr_valid;
	bool is_ap_removal_timer_valid;
	uint16_t ap_removal_timer;

	/* This helper function parses probe request info from the per-STA prof
	 * present (if any) in the Link Info field in the payload of a Multi
	 * Link element (after defragmentation if required). The caller should
	 * pass a copy of the payload so that inline defragmentation of
	 * subelements can be carried out if required. The subelement
	 * defragmentation (if applicable) in this Control Path helper is
	 * required for maintainability, accuracy and eliminating current and
	 * future per-field-access multi-level fragment boundary checks and
	 * adjustments, given the complex format of Multi Link elements. It is
	 * also most likely to be required mainly at the client side.
	 * Fragmentation is currently unlikely to be required for subelements
	 * in Reconfiguration variant Multi-Link elements, but it should be
	 * handled in order to be future ready.
	 */
	if (!linkinfo) {
		mlo_err("linkinfo is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!linkinfo_len) {
		mlo_err("linkinfo_len is zero");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!reconfig_info) {
		mlo_err("ML reconfig info is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	reconfig_info->num_links = 0;
	linkinfo_currpos = linkinfo;
	linkinfo_remlen = linkinfo_len;

	while (linkinfo_remlen) {
		if (linkinfo_remlen <  sizeof(struct subelem_header)) {
			mlo_err_rl("Remaining length in link info %zu octets is smaller than subelement header length %zu octets",
				   linkinfo_remlen,
				   sizeof(struct subelem_header));
			return QDF_STATUS_E_PROTO;
		}

		subelemid = linkinfo_currpos[ID_POS];
		is_subelemfragseq = false;
		subelemseqtotallen = 0;
		subelemseqpayloadlen = 0;

		ret = wlan_get_subelem_fragseq_info(WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
						    linkinfo_currpos,
						    linkinfo_remlen,
						    &is_subelemfragseq,
						    &subelemseqtotallen,
						    &subelemseqpayloadlen);
		if (QDF_IS_STATUS_ERROR(ret))
			return ret;

		if (qdf_unlikely(is_subelemfragseq)) {
			if (!subelemseqpayloadlen) {
				mlo_err_rl("Subelement fragment sequence payload is reported as 0, investigate");
				return QDF_STATUS_E_FAILURE;
			}

			mlo_debug("Subelement fragment sequence found with payload len %zu",
				  subelemseqpayloadlen);

			ret = wlan_defrag_subelem_fragseq(true,
							  WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
							  linkinfo_currpos,
							  linkinfo_remlen,
							  NULL,
							  0,
							  &defragpayload_len);

			if (QDF_IS_STATUS_ERROR(ret))
				return ret;

			if (defragpayload_len != subelemseqpayloadlen) {
				mlo_err_rl("Length of defragmented payload %zu octets is not equal to length of subelement fragment sequence payload %zu octets",
					   defragpayload_len,
					   subelemseqpayloadlen);
				return QDF_STATUS_E_FAILURE;
			}

			/* Adjust linkinfo_remlen to reflect removal of all
			 * subelement headers except the header of the lead
			 * subelement.
			 */
			linkinfo_remlen -= (subelemseqtotallen -
					    subelemseqpayloadlen -
					    sizeof(struct subelem_header));
		} else {
			if (linkinfo_remlen <
				(sizeof(struct subelem_header) +
				linkinfo_currpos[TAG_LEN_POS])) {
				mlo_err_rl("Remaining length in link info %zu octets is smaller than total size of current subelement %zu octets",
					   linkinfo_remlen,
					   sizeof(struct subelem_header) +
					   linkinfo_currpos[TAG_LEN_POS]);
				return QDF_STATUS_E_PROTO;
			}

			subelemseqpayloadlen = linkinfo_currpos[TAG_LEN_POS];
		}

		if (subelemid == WLAN_ML_LINFO_SUBELEMID_PERSTAPROFILE) {
			struct ml_rv_partner_link_info *link_info;
			is_macaddr_valid = false;
			is_ap_removal_timer_valid = false;
			ret = util_parse_rvmlie_perstaprofile_stactrl(linkinfo_currpos +
								      sizeof(struct subelem_header),
								      subelemseqpayloadlen,
								      &linkid,
								      &is_macaddr_valid,
								      &mac_addr,
								      &is_ap_removal_timer_valid,
								      &ap_removal_timer);
			if (QDF_IS_STATUS_ERROR(ret))
				return ret;
			if (reconfig_info->num_links >=
						WLAN_UMAC_MLO_MAX_VDEVS) {
				mlo_err("num_link %d invalid",
					reconfig_info->num_links);
				return QDF_STATUS_E_INVAL;
			}
			link_info =
			&reconfig_info->link_info[reconfig_info->num_links];
			link_info->link_id = linkid;
			link_info->is_ap_removal_timer_p = is_ap_removal_timer_valid;
			if (is_macaddr_valid)
				qdf_copy_macaddr(&link_info->link_mac_addr,
						 &mac_addr);

			if (is_ap_removal_timer_valid)
				link_info->ap_removal_timer = ap_removal_timer;
			else
				mlo_warn_rl("AP removal timer not found in STA Info field of per-STA profile with link ID %u",
					    linkid);

			mlo_debug("Per-STA Profile Link ID: %u AP removal timer present: %d AP removal timer: %u",
				  link_info->link_id,
				  link_info->is_ap_removal_timer_p,
				  link_info->ap_removal_timer);

			reconfig_info->num_links++;
		}

		linkinfo_remlen -= (sizeof(struct subelem_header) +
				    subelemseqpayloadlen);
		linkinfo_currpos += (sizeof(struct subelem_header) +
				     subelemseqpayloadlen);
	}

	mlo_debug("Number of ML probe request links found=%u",
		  reconfig_info->num_links);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS util_get_rvmlie_persta_link_info(uint8_t *mlieseq,
					    qdf_size_t mlieseqlen,
					    struct ml_rv_info *reconfig_info)
{
	struct wlan_ie_multilink *mlie_fixed;
	uint16_t mlcontrol;
	enum wlan_ml_variant variant;
	uint8_t *linkinfo;
	qdf_size_t linkinfo_len;
	struct ml_rv_info rinfo = {0};
	qdf_size_t mlieseqpayloadlen;
	uint8_t *mlieseqpayload_copy;
	bool is_elemfragseq;
	qdf_size_t defragpayload_len;
	qdf_size_t tmplen;
	QDF_STATUS ret;

	if (!mlieseq) {
		mlo_err("Pointer to Multi-Link element sequence is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlieseqlen) {
		mlo_err("Length of Multi-Link element sequence is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (!reconfig_info) {
		mlo_err("reconfig_info is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	reconfig_info->num_links = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink)) {
		mlo_err_rl("Multi-Link element sequence length %zu octets is smaller than required for the fixed portion of Multi-Link element (%zu octets)",
			   mlieseqlen, sizeof(struct wlan_ie_multilink));
		return QDF_STATUS_E_INVAL;
	}

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;
	if (mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM ||
	    mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK) {
		mlo_err("The element is not a Multi-Link element");
		return QDF_STATUS_E_INVAL;
	}

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_RECONFIG) {
		mlo_err("The variant value %u does not correspond to Reconfig Variant value %u",
			variant, WLAN_ML_VARIANT_RECONFIG);
		return QDF_STATUS_E_INVAL;
	}

	mlieseqpayloadlen = 0;
	tmplen = 0;
	is_elemfragseq = false;

	ret = wlan_get_elem_fragseq_info(mlieseq,
					 mlieseqlen,
					 &is_elemfragseq,
					 &tmplen,
					 &mlieseqpayloadlen);

	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	if (qdf_unlikely(is_elemfragseq)) {
		if (tmplen != mlieseqlen) {
			mlo_err_rl("Mismatch in values of element fragment sequence total length. Val per frag info determination: %zu octets, val passed as arg: %zu octets",
				   tmplen, mlieseqlen);
			return QDF_STATUS_E_INVAL;
		}

		if (!mlieseqpayloadlen) {
			mlo_err_rl("Multi-Link element fragment sequence payload is reported as 0, investigate");
			return QDF_STATUS_E_FAILURE;
		}

		mlo_debug("Multi-Link element fragment sequence found with payload len %zu",
			  mlieseqpayloadlen);
	} else {
		if (mlieseqlen > (sizeof(struct ie_header) + WLAN_MAX_IE_LEN)) {
			mlo_err_rl("Expected presence of valid fragment sequence since Multi-Link element sequence length %zu octets is larger than frag threshold of %zu octets, however no valid fragment sequence found",
				   mlieseqlen,
				   sizeof(struct ie_header) + WLAN_MAX_IE_LEN);
			return QDF_STATUS_E_FAILURE;
		}

		mlieseqpayloadlen = mlieseqlen - (sizeof(struct ie_header) + 1);
	}

	mlieseqpayload_copy = qdf_mem_malloc(mlieseqpayloadlen);

	if (!mlieseqpayload_copy) {
		mlo_err_rl("Could not allocate memory for Multi-Link element payload copy");
		return QDF_STATUS_E_NOMEM;
	}

	if (qdf_unlikely(is_elemfragseq)) {
		ret = wlan_defrag_elem_fragseq(false,
					       mlieseq,
					       mlieseqlen,
					       mlieseqpayload_copy,
					       mlieseqpayloadlen,
					       &defragpayload_len);
		if (QDF_IS_STATUS_ERROR(ret)) {
			qdf_mem_free(mlieseqpayload_copy);
			return ret;
		}

		if (defragpayload_len != mlieseqpayloadlen) {
			mlo_err_rl("Length of de-fragmented payload %zu octets is not equal to length of Multi-Link element fragment sequence payload %zu octets",
				   defragpayload_len, mlieseqpayloadlen);
			qdf_mem_free(mlieseqpayload_copy);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		qdf_mem_copy(mlieseqpayload_copy,
			     mlieseq + sizeof(struct ie_header) + 1,
			     mlieseqpayloadlen);
	}

	linkinfo = NULL;
	linkinfo_len = 0;

	ret = util_parse_rv_multi_link_ctrl(mlieseqpayload_copy,
					    mlieseqpayloadlen,
					    &linkinfo,
					    &linkinfo_len);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(mlieseqpayload_copy);
		return ret;
	}

	/* In case Link Info is absent, the number of links will remain
	 * zero.
	 */
	if (!linkinfo) {
		qdf_mem_free(mlieseqpayload_copy);
		return QDF_STATUS_SUCCESS;
	}

	ret = util_parse_rv_info_from_linkinfo(linkinfo, linkinfo_len, &rinfo);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(mlieseqpayload_copy);
		return ret;
	}

	qdf_mem_copy(reconfig_info, &rinfo, sizeof(*reconfig_info));
	qdf_mem_free(mlieseqpayload_copy);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_pa_multi_link_ctrl(uint8_t *mlieseqpayload,
			      qdf_size_t mlieseqpayloadlen,
			      uint8_t **link_info,
			      qdf_size_t *link_info_len)
{
	qdf_size_t parsed_payload_len;

	/* This helper returns the location(s) and length(s) of (sub)field(s)
	 * inferable after parsing the Multi Link element Control field. These
	 * location(s) and length(s) is/are in reference to the payload section
	 * of the Multi Link element (after defragmentation, if applicable).
	 * Here, the payload is the point after the element ID extension of the
	 * Multi Link element, and includes the payloads of all subsequent
	 * fragments (if any) but not the headers of those fragments.
	 *
	 * Currently, the helper returns the location and length of the Link
	 * Info field in the Multi Link element sequence. Other (sub)field(s)
	 * can be added later as required.
	 */
	if (!mlieseqpayload) {
		mlo_err("ML seq payload pointer is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlieseqpayloadlen) {
		mlo_err("ML seq payload len is 0");
		return QDF_STATUS_E_INVAL;
	}

	if (mlieseqpayloadlen < WLAN_ML_CTRL_SIZE) {
		mlo_err_rl("ML seq payload len %zu < ML Control size %u",
			   mlieseqpayloadlen, WLAN_ML_CTRL_SIZE);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len = 0;

	parsed_payload_len += WLAN_ML_CTRL_SIZE;

	if (mlieseqpayloadlen <
		(parsed_payload_len +
		WLAN_ML_PAV_CINFO_LENGTH_MAX)) {
		mlo_err_rl("ML seq payload len %zu insufficient for MLD cmn size %u after parsed payload len %zu.",
			   mlieseqpayloadlen,
			   WLAN_ML_PAV_CINFO_LENGTH_MAX,
			   parsed_payload_len);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len += QDF_MAC_ADDR_SIZE + WLAN_ML_PAV_CINFO_LENGTH_SIZE;

	if (link_info_len) {
		*link_info_len = mlieseqpayloadlen - parsed_payload_len;
		mlo_debug("link_info_len:%zu, parsed_payload_len:%zu",
			  *link_info_len, parsed_payload_len);
	}

	if (mlieseqpayloadlen == parsed_payload_len) {
		mlo_debug("No Link Info field present");
		if (link_info)
			*link_info = NULL;

		return QDF_STATUS_SUCCESS;
	}

	if (link_info)
		*link_info = mlieseqpayload + parsed_payload_len;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_pamlie_perstaprofile_stactrl(uint8_t *subelempayload,
					qdf_size_t subelempayloadlen,
					struct ml_pa_partner_link_info *pa_link_info)
{
	qdf_size_t parsed_payload_len = 0;
	uint16_t stacontrol;
	struct ie_header *ie;
	struct extn_ie_header *extn_ie;

	/* This helper returns the location(s) and where required, the length(s)
	 * of (sub)field(s) inferable after parsing the STA Control field in the
	 * per-STA profile subelement. These location(s) and length(s) is/are in
	 * reference to the payload section of the per-STA profile subelement
	 * (after defragmentation, if applicable).  Here, the payload is the
	 * point after the subelement length in the subelement, and includes the
	 * payloads of all subsequent fragments (if any) but not the headers of
	 * those fragments.
	 *
	 * Currently, the helper returns the priority access link information
	 * for all parner links.
	 */
	if (!subelempayload) {
		mlo_err("Pointer to subelement payload is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!subelempayloadlen) {
		mlo_err("Length of subelement payload is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (subelempayloadlen < WLAN_ML_PAV_LINFO_PERSTAPROF_STACTRL_SIZE) {
		mlo_err_rl("Subelement payload length %zu octets is smaller than STA control field of per-STA profile subelement %u octets",
			   subelempayloadlen,
			   WLAN_ML_PAV_LINFO_PERSTAPROF_STACTRL_SIZE);
		return QDF_STATUS_E_PROTO;
	}

	parsed_payload_len = 0;
	qdf_mem_copy(&stacontrol,
		     subelempayload,
		     WLAN_ML_PAV_LINFO_PERSTAPROF_STACTRL_SIZE);

	stacontrol = qdf_le16_to_cpu(stacontrol);
	parsed_payload_len += WLAN_ML_PAV_LINFO_PERSTAPROF_STACTRL_SIZE;

	subelempayload += WLAN_ML_PAV_LINFO_PERSTAPROF_STACTRL_SIZE;

	pa_link_info->link_id =
		QDF_GET_BITS(stacontrol,
			     WLAN_ML_PAV_LINFO_PERSTAPROF_STACTRL_LINKID_IDX,
			     WLAN_ML_PAV_LINFO_PERSTAPROF_STACTRL_LINKID_BITS);

	pa_link_info->edca_ie_present = false;
	pa_link_info->ven_wme_ie_present = false;
	pa_link_info->muedca_ie_present = false;

	do {
		if (subelempayloadlen <
			(parsed_payload_len +
			sizeof(struct ie_header))) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain min ie length %zu after parsed payload length of %zu octets",
				   subelempayloadlen,
				   sizeof(struct ie_header),
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		ie = (struct ie_header *)subelempayload;

		if (subelempayloadlen <
			(parsed_payload_len +
			(sizeof(struct ie_header) + ie->ie_len))) {
			mlo_err_rl("Length of subelement payload %zu octets not sufficient to contain ie length %zu after parsed payload length of %zu octets",
				   subelempayloadlen,
				   sizeof(struct ie_header) + ie->ie_len,
				   parsed_payload_len);
			return QDF_STATUS_E_PROTO;
		}

		switch (ie->ie_id) {
		case WLAN_ELEMID_EDCAPARMS:
			if (pa_link_info->ven_wme_ie_present) {
				/* WME parameters already present
				 * use that one instead of EDCA */
				break;
			}
			if (ie->ie_len == (sizeof(struct edca_ie) -
			    sizeof(struct ie_header))) {
				pa_link_info->edca_ie_present = true;
				qdf_mem_copy(&pa_link_info->edca,
					     subelempayload,
					     sizeof(struct edca_ie));
			} else {
				epcs_debug("Invalid edca length %d in PAV IE",
					   ie->ie_len);
			}
			break;
		case WLAN_ELEMID_VENDOR:
			if (is_wme_param((uint8_t *)ie) &&
			    (ie->ie_len == WLAN_VENDOR_WME_IE_LEN)) {
				pa_link_info->ven_wme_ie_present = true;
				qdf_mem_copy(&pa_link_info->ven_wme_ie_bytes,
					     subelempayload,
					     sizeof(WLAN_VENDOR_WME_IE_LEN +
						    sizeof(struct ie_header)));
				pa_link_info->edca_ie_present = false;
			} else {
				epcs_debug("Unrelated Venfor IE reecived ie_id %d ie_len %d",
					   ie->ie_id,
					   ie->ie_len);
			}
			break;
		case WLAN_ELEMID_EXTN_ELEM:
			extn_ie = (struct extn_ie_header *)ie;
			/**
			 * Zero IE len means there is no IE contents (EXT ID)
			 * and so, if IE is dereferenced after IE len then it
			 * can leads to out of bound error.
			 * | IE ID | IE len | EXT ID |
			 */
			if (!extn_ie->ie_len) {
				mlo_err_rl("extn element has zero len");
				return QDF_STATUS_E_PROTO;
			}
			switch (extn_ie->ie_extn_id) {
			case WLAN_EXTN_ELEMID_MUEDCA:
				if (extn_ie->ie_len == WLAN_MAX_MUEDCA_IE_LEN) {
					pa_link_info->muedca_ie_present = true;
					qdf_mem_copy(&pa_link_info->muedca,
						     subelempayload,
						     sizeof(struct muedca_ie));
				} else {
					epcs_debug("Invalid muedca length %d in PAV IE",
						   ie->ie_len);
				}
				break;
			default:
				epcs_debug("Unrelated Extn IE reecived ie_id %d ie_len %d extid %d IN PAV IE",
					   ie->ie_id,
					   ie->ie_len,
					   extn_ie->ie_extn_id);
				break;
			}
			break;
		default:
			epcs_debug("Unrelated IE reecived ie_id %d ie_len %d in PAV IE",
				   ie->ie_id,
				   ie->ie_len);
			break;
		}
		subelempayload += ie->ie_len + sizeof(struct ie_header);
		parsed_payload_len += ie->ie_len + sizeof(struct ie_header);
	} while (parsed_payload_len < subelempayloadlen);

	if (parsed_payload_len != subelempayloadlen)
		epcs_debug("Error in processing per sta profile of PA ML IE %zu %zu",
			   parsed_payload_len,
			   subelempayloadlen);

	epcs_debug("Link id %d presence of edca %d muedca %d wme %d",
		   pa_link_info->link_id,
		   pa_link_info->edca_ie_present,
		   pa_link_info->muedca_ie_present,
		   pa_link_info->ven_wme_ie_present);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
util_parse_pa_info_from_linkinfo(uint8_t *linkinfo,
				 qdf_size_t linkinfo_len,
				 struct ml_pa_info *pa_info)
{
	uint8_t *linkinfo_currpos;
	qdf_size_t linkinfo_remlen;
	bool is_subelemfragseq;
	uint8_t subelemid;
	qdf_size_t subelemseqtotallen;
	qdf_size_t subelemseqpayloadlen;
	qdf_size_t defragpayload_len;
	QDF_STATUS ret;

	/* This helper function parses priority access info from the per-STA
	 * prof present (if any) in the Link Info field in the payload of a
	 * Multi Link element (after defragmentation if required). The caller
	 * should pass a copy of the payload so that inline defragmentation of
	 * subelements can be carried out if required. The subelement
	 * defragmentation (if applicable) in this Control Path helper is
	 * required for maintainability, accuracy and eliminating current and
	 * future per-field-access multi-level fragment boundary checks and
	 * adjustments, given the complex format of Multi Link elements. It is
	 * also most likely to be required mainly at the client side.
	 * Fragmentation is currently unlikely to be required for subelements
	 * in Reconfiguration variant Multi-Link elements, but it should be
	 * handled in order to be future ready.
	 */
	if (!linkinfo) {
		mlo_err("linkinfo is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!linkinfo_len) {
		mlo_err("linkinfo_len is zero");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!pa_info) {
		mlo_err("ML pa info is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pa_info->num_links = 0;
	linkinfo_currpos = linkinfo;
	linkinfo_remlen = linkinfo_len;

	while (linkinfo_remlen &&
	       pa_info->num_links < QDF_ARRAY_SIZE(pa_info->link_info)) {
		if (linkinfo_remlen <  sizeof(struct subelem_header)) {
			mlo_err_rl("Remaining length in link info %zu octets is smaller than subelement header length %zu octets",
				   linkinfo_remlen,
				   sizeof(struct subelem_header));
			return QDF_STATUS_E_PROTO;
		}

		subelemid = linkinfo_currpos[ID_POS];
		is_subelemfragseq = false;
		subelemseqtotallen = 0;
		subelemseqpayloadlen = 0;

		ret = wlan_get_subelem_fragseq_info(WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
						    linkinfo_currpos,
						    linkinfo_remlen,
						    &is_subelemfragseq,
						    &subelemseqtotallen,
						    &subelemseqpayloadlen);
		if (QDF_IS_STATUS_ERROR(ret))
			return ret;

		if (qdf_unlikely(is_subelemfragseq)) {
			if (!subelemseqpayloadlen) {
				mlo_err_rl("Subelement fragment sequence payload is reported as 0, investigate");
				return QDF_STATUS_E_FAILURE;
			}

			mlo_debug("Subelement fragment sequence found with payload len %zu",
				  subelemseqpayloadlen);

			ret = wlan_defrag_subelem_fragseq(true,
							  WLAN_ML_LINFO_SUBELEMID_FRAGMENT,
							  linkinfo_currpos,
							  linkinfo_remlen,
							  NULL,
							  0,
							  &defragpayload_len);

			if (QDF_IS_STATUS_ERROR(ret))
				return ret;

			if (defragpayload_len != subelemseqpayloadlen) {
				mlo_err_rl("Length of defragmented payload %zu octets is not equal to length of subelement fragment sequence payload %zu octets",
					   defragpayload_len,
					   subelemseqpayloadlen);
				return QDF_STATUS_E_FAILURE;
			}

			/* Adjust linkinfo_remlen to reflect removal of all
			 * subelement headers except the header of the lead
			 * subelement.
			 */
			linkinfo_remlen -= (subelemseqtotallen -
					    subelemseqpayloadlen -
					    sizeof(struct subelem_header));
		} else {
			if (linkinfo_remlen <
				(sizeof(struct subelem_header) +
				linkinfo_currpos[TAG_LEN_POS])) {
				mlo_err_rl("Remaining length in link info %zu octets is smaller than total size of current subelement %zu octets",
					   linkinfo_remlen,
					   sizeof(struct subelem_header) +
					   linkinfo_currpos[TAG_LEN_POS]);
				return QDF_STATUS_E_PROTO;
			}

			subelemseqpayloadlen = linkinfo_currpos[TAG_LEN_POS];
		}

		if (subelemid == WLAN_ML_LINFO_SUBELEMID_PERSTAPROFILE) {
			struct ml_pa_partner_link_info *link_info =
					&pa_info->link_info[pa_info->num_links];
			ret = util_parse_pamlie_perstaprofile_stactrl(linkinfo_currpos +
								      sizeof(struct subelem_header),
								      subelemseqpayloadlen,
								      link_info);
			if (QDF_IS_STATUS_ERROR(ret))
				return ret;
		}

		pa_info->num_links++;

		linkinfo_remlen -= (sizeof(struct subelem_header) +
				    subelemseqpayloadlen);
		linkinfo_currpos += (sizeof(struct subelem_header) +
				     subelemseqpayloadlen);
	}

	mlo_debug("Number of ML probe request links found=%u",
		  pa_info->num_links);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
util_get_pav_mlie_link_info(uint8_t *mlieseq,
			    qdf_size_t mlieseqlen,
			    struct ml_pa_info *pa_info)
{
	struct wlan_ie_multilink *mlie_fixed;
	uint16_t mlcontrol;
	enum wlan_ml_variant variant;
	uint8_t *linkinfo;
	qdf_size_t linkinfo_len;
	struct ml_pa_info painfo = {0};
	qdf_size_t mlieseqpayloadlen;
	uint8_t *mlieseqpayload_copy;
	bool is_elemfragseq;
	qdf_size_t defragpayload_len;
	qdf_size_t tmplen;
	QDF_STATUS ret;

	if (!mlieseq) {
		mlo_err("Pointer to Multi-Link element sequence is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!mlieseqlen) {
		mlo_err("Length of Multi-Link element sequence is zero");
		return QDF_STATUS_E_INVAL;
	}

	if (!pa_info) {
		mlo_err("pa_info is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pa_info->num_links = 0;

	if (mlieseqlen < sizeof(struct wlan_ie_multilink)) {
		mlo_err_rl("Multi-Link element sequence length %zu octets is smaller than required for the fixed portion of Multi-Link element (%zu octets)",
			   mlieseqlen, sizeof(struct wlan_ie_multilink));
		return QDF_STATUS_E_INVAL;
	}

	mlie_fixed = (struct wlan_ie_multilink *)mlieseq;
	if (mlie_fixed->elem_id != WLAN_ELEMID_EXTN_ELEM ||
	    mlie_fixed->elem_id_ext != WLAN_EXTN_ELEMID_MULTI_LINK) {
		mlo_err("The element is not a Multi-Link element");
		return QDF_STATUS_E_INVAL;
	}

	mlcontrol = qdf_le16_to_cpu(mlie_fixed->mlcontrol);

	variant = QDF_GET_BITS(mlcontrol, WLAN_ML_CTRL_TYPE_IDX,
			       WLAN_ML_CTRL_TYPE_BITS);

	if (variant != WLAN_ML_VARIANT_PRIORITYACCESS) {
		mlo_err("The variant value %u does not correspond to priority access Variant value %u",
			variant, WLAN_ML_VARIANT_PRIORITYACCESS);
		return QDF_STATUS_E_INVAL;
	}

	mlieseqpayloadlen = 0;
	tmplen = 0;
	is_elemfragseq = false;

	ret = wlan_get_elem_fragseq_info(mlieseq,
					 mlieseqlen,
					 &is_elemfragseq,
					 &tmplen,
					 &mlieseqpayloadlen);

	if (QDF_IS_STATUS_ERROR(ret))
		return ret;

	if (qdf_unlikely(is_elemfragseq)) {
		if (tmplen != mlieseqlen) {
			mlo_err_rl("Mismatch in values of element fragment sequence total length. Val per frag info determination: %zu octets, val passed as arg: %zu octets",
				   tmplen, mlieseqlen);
			return QDF_STATUS_E_INVAL;
		}

		if (!mlieseqpayloadlen) {
			mlo_err_rl("Multi-Link element fragment sequence payload is reported as 0, investigate");
			return QDF_STATUS_E_FAILURE;
		}

		mlo_debug("Multi-Link element fragment sequence found with payload len %zu",
			  mlieseqpayloadlen);
	} else {
		if (mlieseqlen > (sizeof(struct ie_header) + WLAN_MAX_IE_LEN)) {
			mlo_err_rl("Expected presence of valid fragment sequence since Multi-Link element sequence length %zu octets is larger than frag threshold of %zu octets, however no valid fragment sequence found",
				   mlieseqlen,
				   sizeof(struct ie_header) + WLAN_MAX_IE_LEN);
			return QDF_STATUS_E_FAILURE;
		}

		mlieseqpayloadlen = mlieseqlen - (sizeof(struct ie_header) + 1);
	}

	mlieseqpayload_copy = qdf_mem_malloc(mlieseqpayloadlen);

	if (!mlieseqpayload_copy) {
		mlo_err_rl("Could not allocate memory for Multi-Link element payload copy");
		return QDF_STATUS_E_NOMEM;
	}

	if (qdf_unlikely(is_elemfragseq)) {
		ret = wlan_defrag_elem_fragseq(false,
					       mlieseq,
					       mlieseqlen,
					       mlieseqpayload_copy,
					       mlieseqpayloadlen,
					       &defragpayload_len);
		if (QDF_IS_STATUS_ERROR(ret)) {
			qdf_mem_free(mlieseqpayload_copy);
			return ret;
		}

		if (defragpayload_len != mlieseqpayloadlen) {
			mlo_err_rl("Length of de-fragmented payload %zu octets is not equal to length of Multi-Link element fragment sequence payload %zu octets",
				   defragpayload_len, mlieseqpayloadlen);
			qdf_mem_free(mlieseqpayload_copy);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		qdf_mem_copy(mlieseqpayload_copy,
			     mlieseq + sizeof(struct ie_header) + 1,
			     mlieseqpayloadlen);
	}

	linkinfo = NULL;
	linkinfo_len = 0;

	ret = util_parse_pa_multi_link_ctrl(mlieseqpayload_copy,
					    mlieseqpayloadlen,
					    &linkinfo,
					    &linkinfo_len);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(mlieseqpayload_copy);
		return ret;
	}

	/* In case Link Info is absent, the number of links will remain
	 * zero.
	 */
	if (!linkinfo) {
		qdf_mem_free(mlieseqpayload_copy);
		return QDF_STATUS_SUCCESS;
	}

	mlo_debug("Dumping hex of link info after parsing Multi-Link element control");
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_MLO, QDF_TRACE_LEVEL_ERROR,
			   linkinfo, linkinfo_len);

	ret = util_parse_pa_info_from_linkinfo(linkinfo, linkinfo_len, &painfo);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(mlieseqpayload_copy);
		return ret;
	}

	qdf_mem_copy(pa_info, &painfo, sizeof(painfo));
	qdf_mem_free(mlieseqpayload_copy);

	return QDF_STATUS_SUCCESS;
}

#endif

#ifdef WLAN_FEATURE_11BE

QDF_STATUS util_add_bw_ind(struct wlan_ie_bw_ind *bw_ind, uint8_t ccfs0,
			   uint8_t ccfs1, enum phy_ch_width ch_width,
			   uint16_t puncture_bitmap, int *bw_ind_len)
{
	uint8_t bw_ind_width;

	if (!bw_ind) {
		mlo_err("Pointer to bandwidth indiaction element is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!bw_ind_len) {
		mlo_err("Length of bandwidth indaication element is Zero");
		return QDF_STATUS_E_INVAL;
	}

	switch (ch_width) {
	case CH_WIDTH_20MHZ:
		bw_ind_width = IEEE80211_11BEOP_CHWIDTH_20;
		break;
	case CH_WIDTH_40MHZ:
		bw_ind_width = IEEE80211_11BEOP_CHWIDTH_40;
		break;
	case CH_WIDTH_80MHZ:
		bw_ind_width = IEEE80211_11BEOP_CHWIDTH_80;
		break;
	case CH_WIDTH_160MHZ:
		bw_ind_width = IEEE80211_11BEOP_CHWIDTH_160;
		break;
	case CH_WIDTH_320MHZ:
		bw_ind_width = IEEE80211_11BEOP_CHWIDTH_320;
		break;
	default:
		bw_ind_width = IEEE80211_11BEOP_CHWIDTH_20;
	}

	bw_ind->elem_id = WLAN_ELEMID_EXTN_ELEM;
	*bw_ind_len = WLAN_BW_IND_IE_MAX_LEN;
	bw_ind->elem_len = WLAN_BW_IND_IE_MAX_LEN - WLAN_IE_HDR_LEN;
	bw_ind->elem_id_extn = WLAN_EXTN_ELEMID_BW_IND;
	bw_ind->ccfs0 = ccfs0;
	bw_ind->ccfs1 = ccfs1;
	QDF_SET_BITS(bw_ind->control, BW_IND_CHAN_WIDTH_IDX,
		     BW_IND_CHAN_WIDTH_BITS, bw_ind_width);

	if (puncture_bitmap) {
		bw_ind->disabled_sub_chan_bitmap[0] =
			QDF_GET_BITS(puncture_bitmap, 0, 8);
		bw_ind->disabled_sub_chan_bitmap[1] =
			QDF_GET_BITS(puncture_bitmap, 8, 8);
		QDF_SET_BITS(bw_ind->bw_ind_param,
			     BW_IND_PARAM_DISABLED_SC_BITMAP_PRESENT_IDX,
			     BW_IND_PARAM_DISABLED_SC_BITMAP_PRESENT_BITS, 1);
	} else {
		QDF_SET_BITS(bw_ind->bw_ind_param,
			     BW_IND_PARAM_DISABLED_SC_BITMAP_PRESENT_IDX,
			     BW_IND_PARAM_DISABLED_SC_BITMAP_PRESENT_BITS, 0);
		bw_ind->elem_len -=
			QDF_ARRAY_SIZE(bw_ind->disabled_sub_chan_bitmap);
		*bw_ind_len -=
			QDF_ARRAY_SIZE(bw_ind->disabled_sub_chan_bitmap);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS util_parse_bw_ind(struct wlan_ie_bw_ind *bw_ind, uint8_t *ccfs0,
			     uint8_t *ccfs1, enum phy_ch_width *ch_width,
			     uint16_t *puncture_bitmap)
{
	uint8_t bw_ind_width;

	if (!bw_ind) {
		mlo_err("Pointer to bandwidth indiaction element is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	*ccfs0 = bw_ind->ccfs0;
	*ccfs1 = bw_ind->ccfs1;
	bw_ind_width = QDF_GET_BITS(bw_ind->control, BW_IND_CHAN_WIDTH_IDX,
				    BW_IND_CHAN_WIDTH_BITS);

	switch (bw_ind_width) {
	case IEEE80211_11BEOP_CHWIDTH_20:
		*ch_width = CH_WIDTH_20MHZ;
		break;
	case IEEE80211_11BEOP_CHWIDTH_40:
		*ch_width = CH_WIDTH_40MHZ;
		break;
	case IEEE80211_11BEOP_CHWIDTH_80:
		*ch_width = CH_WIDTH_80MHZ;
		break;
	case IEEE80211_11BEOP_CHWIDTH_160:
		*ch_width = CH_WIDTH_160MHZ;
		break;
	case IEEE80211_11BEOP_CHWIDTH_320:
		*ch_width = CH_WIDTH_320MHZ;
		break;
	default:
		*ch_width = CH_WIDTH_20MHZ;
	}

	if (QDF_GET_BITS(bw_ind->bw_ind_param,
			 BW_IND_PARAM_DISABLED_SC_BITMAP_PRESENT_IDX,
			 BW_IND_PARAM_DISABLED_SC_BITMAP_PRESENT_BITS)) {
		QDF_SET_BITS(*puncture_bitmap, 0, 8,
			     bw_ind->disabled_sub_chan_bitmap[0]);
		QDF_SET_BITS(*puncture_bitmap, 8, 8,
			     bw_ind->disabled_sub_chan_bitmap[1]);
	} else {
		*puncture_bitmap = 0;
	}

	return QDF_STATUS_SUCCESS;
}
#endif

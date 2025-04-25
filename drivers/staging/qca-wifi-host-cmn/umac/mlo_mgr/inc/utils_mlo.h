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
 * DOC: contains MLO manager containing util public api's
 */
#ifndef _WLAN_UTILS_MLO_H_
#define _WLAN_UTILS_MLO_H_

#include <wlan_cmn_ieee80211.h>
#include "wlan_mlo_mgr_public_structs.h"
#include <wlan_cm_ucfg_api.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_mlo_epcs.h>

#ifdef WLAN_FEATURE_11BE_MLO

#define MLO_LINKSPECIFIC_ASSOC_REQ_FC0  0x00
#define MLO_LINKSPECIFIC_ASSOC_REQ_FC1  0x00
#define MLO_LINKSPECIFIC_ASSOC_RESP_FC0 0x10
#define MLO_LINKSPECIFIC_ASSOC_RESP_FC1 0x00
#define MLO_LINKSPECIFIC_PROBE_RESP_FC0 0x50
#define MLO_LINKSPECIFIC_PROBE_RESP_FC1 0x00

/**
 * util_gen_link_assoc_req() - Generate link specific assoc request
 * @frame: Pointer to original association request. This should not contain the
 * 802.11 header, and must start from the fixed fields in the association
 * request. This is required due to some caller semantics built into the end to
 * end design.
 * @frame_len: Length of original association request
 * @isreassoc: Whether this is a re-association request
 * @link_id: Link ID for secondary links
 * @link_addr: Secondary link's MAC address
 * @link_frame: Generated secondary link specific association request. Note that
 * this will start from the 802.11 header (unlike the original association
 * request). This should be ignored in the case of failure.
 * @link_frame_maxsize: Maximum size of generated secondary link specific
 * association request
 * @link_frame_len: Pointer to location where populated length of generated
 * secondary link specific association request should be written. This should be
 * ignored in the case of failure.
 *
 * Generate a link specific logically equivalent association request for the
 * secondary link from the original association request containing a Multi-Link
 * element. This applies to both association and re-association requests.
 * Currently, only two link MLO is supported.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure.
 */
QDF_STATUS
util_gen_link_assoc_req(uint8_t *frame, qdf_size_t frame_len, bool isreassoc,
			uint8_t link_id,
			struct qdf_mac_addr link_addr,
			uint8_t *link_frame,
			qdf_size_t link_frame_maxsize,
			qdf_size_t *link_frame_len);

/**
 * util_gen_link_assoc_rsp() - Generate link specific assoc response
 * @frame: Pointer to original association response. This should not contain the
 * 802.11 header, and must start from the fixed fields in the association
 * response. This is required due to some caller semantics built into the end to
 * end design.
 * @frame_len: Length of original association response
 * @isreassoc: Whether this is a re-association response
 * @link_id: Link ID for secondary links
 * @link_addr: Secondary link's MAC address
 * @link_frame: Generated secondary link specific association response. Note
 * that this will start from the 802.11 header (unlike the original association
 * response). This should be ignored in the case of failure.
 * @link_frame_maxsize: Maximum size of generated secondary link specific
 * association response
 * @link_frame_len: Pointer to location where populated length of generated
 * secondary link specific association response should be written. This should
 * be ignored in the case of failure.
 *
 * Generate a link specific logically equivalent association response for the
 * secondary link from the original association response containing a Multi-Link
 * element. This applies to both association and re-association responses.
 * Currently, only two link MLO is supported.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure.
 */
QDF_STATUS
util_gen_link_assoc_rsp(uint8_t *frame, qdf_size_t frame_len, bool isreassoc,
			uint8_t link_id,
			struct qdf_mac_addr link_addr,
			uint8_t *link_frame,
			qdf_size_t link_frame_maxsize,
			qdf_size_t *link_frame_len);

/**
 * util_gen_link_probe_rsp() - Generate link specific probe response
 * @frame: Pointer to original probe response. This should not contain the
 * 802.11 header, and must start from the fixed fields in the probe
 * response. This is required due to some caller semantics built into the end to
 * end design.
 * @frame_len: Length of original probe response
 * @link_addr: Secondary link's MAC address
 * @link_id: Link ID for secondary links
 * @link_frame: Generated secondary link specific probe response. Note
 * that this will start from the 802.11 header (unlike the original probe
 * response). This should be ignored in the case of failure.
 * @link_frame_maxsize: Maximum size of generated secondary link specific
 * probe response
 * @link_frame_len: Pointer to location where populated length of generated
 * secondary link specific probe response should be written. This should
 * be ignored in the case of failure.
 *
 * Generate a link specific logically equivalent probe response for the
 * secondary link from the original probe response containing a Multi-Link
 * element. This applies to both probe responses.
 * Currently, only two link MLO is supported.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure.
 */
QDF_STATUS
util_gen_link_probe_rsp(uint8_t *frame, qdf_size_t frame_len,
			uint8_t link_id,
			struct qdf_mac_addr link_addr,
			uint8_t *link_frame,
			qdf_size_t link_frame_maxsize,
			qdf_size_t *link_frame_len);

/**
 * util_find_mlie - Find the first Multi-Link element or the start of the first
 * Multi-Link element fragment sequence in a given buffer containing elements,
 * if a Multi-Link element or element fragment sequence exists in the given
 * buffer.
 *
 * @buf: Buffer to be searched for the Multi-Link element or the start of the
 * Multi-Link element fragment sequence
 * @buflen: Length of the buffer
 * @mlieseq: Pointer to location where the starting address of the Multi-Link
 * element or Multi-Link element fragment sequence should be updated if found
 * in the given buffer. The value NULL will be updated to this location if the
 * element or element fragment sequence is not found. This should be ignored by
 * the caller if the function returns error.
 * @mlieseqlen: Pointer to location where the total length of the Multi-Link
 * element or Multi-Link element fragment sequence should be updated if found
 * in the given buffer. This should be ignored by the caller if the function
 * returns error, or if the function indicates that the element or element
 * fragment sequence was not found by providing a starting address of NULL.
 *
 * Find the first Multi-Link element or the start of the first Multi-Link
 * element fragment sequence in a given buffer containing elements, if a
 * Multi-Link element or element fragment sequence exists in the given buffer.
 * The buffer should contain only 802.11 Information elements, and thus should
 * not contain other information like 802.11 header, 802.11 frame body
 * components like fields that are not elements (e.g. Capability Information
 * field, Beacon Interval field), etc.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_find_mlie(uint8_t *buf, qdf_size_t buflen, uint8_t **mlieseq,
	       qdf_size_t *mlieseqlen);

/**
 * util_find_mlie_by_variant - Find the first Multi-Link element or the start of
 * the first Multi-Link element fragment sequence in a given buffer containing
 * elements based on variant, if a Multi-Link element or element fragment
 * sequence exists in the given buffer.
 *
 * @buf: Buffer to be searched for the Multi-Link element or the start of the
 * Multi-Link element fragment sequence
 * @buflen: Length of the buffer
 * @mlieseq: Based on the variant, pointer to location where the starting
 * address of the Multi-Link element or Multi-Link element fragment sequence
 * should be updated if found in the given buffer. The value NULL will be
 * updated to this location if the element or element fragment sequence is not
 * found. This should be ignored by the caller if the function returns error.
 * @mlieseqlen: Pointer to location where the total length of the Multi-Link
 * element or Multi-Link element fragment sequence should be updated if found
 * in the given buffer. This should be ignored by the caller if the function
 * returns error, or if the function indicates that the element or element
 * fragment sequence was not found by providing a starting address of NULL.
 * @variant: Multi-Link element variant.  The value should be interpreted by the
 * caller as a member of enum wlan_ml_variant. (This enum is not directly used
 * as an argument, so that non-MLO code that happens to call this function does
 * not need to be aware of the definition of the enum, though such a call would
 * ultimately result in an error).
 *
 * Based on variant, find the Multi-Link element or the start of the Multi-Link
 * element fragment sequence in a given buffer containing elements, if a
 * Multi-Link element or element fragment sequence exists in the given buffer.
 * The buffer should contain only 802.11 Information elements, and thus should
 * not contain other information like 802.11 header, 802.11 frame body
 * components like fields that are not elements (e.g. Capability Information
 * field, Beacon Interval field), etc.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_find_mlie_by_variant(uint8_t *buf, qdf_size_t buflen, uint8_t **mlieseq,
			  qdf_size_t *mlieseqlen, int variant);

/**
 * util_get_mlie_variant() - Get ML IE variant
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @variant: Pointer to the location where the value of the variant should be
 * updated. On success, the value should be interpreted by the caller as a
 * member of enum wlan_ml_variant. (This enum is not directly used as an
 * argument, so that non-MLO code that happens to call this function does not
 * need to be aware of the definition of the enum, though such a call would
 * ultimately result in an error). The value should be ignored by the caller if
 * the function returns error.
 *
 * Get the variant of the given Multi-Link element or element fragment sequence.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_mlie_variant(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		      int *variant);

/**
 * util_get_bvmlie_mldmacaddr() - Get the MLD MAC address
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mldmacaddr: Pointer to the location where the MLD MAC address should be
 * updated. This should be ignored by the caller if the function returns error.
 *
 * Get the MLD MAC address from a given Basic variant Multi-Link element
 * or element fragment sequence.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_bvmlie_mldmacaddr(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			   struct qdf_mac_addr *mldmacaddr);

/**
 * util_get_bvmlie_eml_cap() - Get the EML capabilities
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @eml_cap_found: Pointer to the location where a boolean status should be
 * updated indicating whether the EML cabalility was found or not. This should
 * be ignored by the caller if the function returns error.
 * @eml_cap: Pointer to the location where the EML capabilities should be
 * updated. This should be ignored by the caller if the function indicates
 * that the EML capability was not found.
 *
 * Get the EML capabilities from a given Basic variant Multi-Link element or
 * element fragment sequence.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_bvmlie_eml_cap(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			bool *eml_cap_found,
			uint16_t *eml_cap);

/**
 * util_get_bvmlie_msd_cap() - Get the MSD capabilities for Basic variant
 * MLO IE
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @msd_cap_found: Pointer to the location where a boolean status should be
 * updated indicating whether the MSD cabalility was found or not. This should
 * be ignored by the caller if the function returns error.
 * @msd_cap: Pointer to the location where the MSD capabilities should be
 * updated. This should be ignored by the caller if the function indicates
 * that the MSD capability was not found.
 *
 * Get the MSD capabilities from a given Basic variant Multi-Link element or
 * element fragment sequence.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_bvmlie_msd_cap(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			bool *msd_cap_found, uint16_t *msd_cap);
/**
 * util_get_bvmlie_primary_linkid() - Get the link identifier
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @linkidfound: Pointer to the location where a boolean status should be
 * updated indicating whether the link identifier was found or not. This should
 * be ignored by the caller if the function returns error.
 * @linkid: Pointer to the location where the value of the link identifier
 * should be updated. This should be ignored by the caller if the function
 * returns error, or if the function indicates that the link identifier was not
 * found.
 *
 * Get the link identifier from a given Basic variant Multi-Link element or
 * element fragment sequence, of the AP that transmits the Multi-Link
 * element/element fragment sequence or the nontransmitted BSSID in the same
 * multiple BSSID set as the AP that transmits the Multi-Link element/element
 * fragment sequence and that is affiliated with the MLD that is described in
 * the Multi-Link element.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_bvmlie_primary_linkid(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			       bool *linkidfound, uint8_t *linkid);

/**
 * util_get_mlie_common_info_len() - Get the MLD common info len
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @commoninfo_len: Pointer to the location where the value of the MLD common
 * info len should be updated. This should be ignored by the caller if the
 * function returns error.
 *
 * Get the MLD common info len from Multi-Link element transmitted by the AP.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_mlie_common_info_len(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			      uint8_t *commoninfo_len);

/**
 * util_get_bvmlie_bssparamchangecnt() - Get the MLD BSS PARAM Change Count
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @bssparamchangecntfound: Pointer to the location where a boolean status
 * should be updated indicating whether the MLD BSS PARAM Change Count was
 * found or not. This should be ignored by the caller if the function
 * returns error.
 * @bssparamchangecnt: Pointer to the location where the value of the MLD BSS
 * PARAM Change Count should be updated. This should be ignored by the caller
 * if the function returns error, or if the function indicates that the MLD
 * BSS PARAM Change Count was not found.
 *
 * Get the MLD BSS PARAM Change Count from Multi-Link element transmitted
 * by the AP.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_bvmlie_bssparamchangecnt(uint8_t *mlieseq, qdf_size_t mlieseqlen,
				  bool *bssparamchangecntfound,
				  uint8_t *bssparamchangecnt);

/**
 * util_get_bvmlie_mldcap() - Get the MLD capabilities
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mldcapfound: Pointer to the location where a boolean status should be
 * updated indicating whether the MLD capabilities was found or not. This should
 * be ignored by the caller if the function returns error.
 * @mldcap: Pointer to the location where the value of the MLD capabilities
 * should be updated. This should be ignored by the caller if the function
 * returns error, or if the function indicates that the MLD capabilities was not
 * found.
 *
 * Get the MLD capabilities from a given Basic variant Multi-Link element or
 * element fragment sequence, of the AP that transmits the Multi-Link
 * element/element fragment sequence or the nontransmitted BSSID in the same
 * multiple BSSID set as the AP that transmits the Multi-Link element/element
 * fragment sequence and that is affiliated with the MLD that is described in
 * the Multi-Link element.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_bvmlie_mldcap(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		       bool *mldcapfound, uint16_t *mldcap);
/**
 * util_get_bvmlie_ext_mld_cap_op_info() - Get Ext MLD Capabilities and
 * operation
 * @mlie_seq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlie_seqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @ext_mld_cap_found: Pointer to the location where a boolean status should be
 * updated indicating whether the Ext MLD capabilities was found or not.
 * This should be ignored by the caller if the function returns error.
 * @ext_mld_cap: Pointer to the location where the value of the Ext MLD
 * capabilities should be updated. This should be ignored by the caller if the
 * function returns error, or if the function indicates that the MLD
 * capabilities was not found.
 *
 * Get the Ext MLD capabilities from a given Basic variant Multi-Link element or
 * element fragment sequence, of the AP that transmits the Multi-Link element/
 * element fragment sequence or the non-transmitted BSSID in the same
 * multiple BSSID set as the AP that transmits the Multi-Link element/element
 * fragment sequence and that is affiliated with the MLD that is described in
 * the Multi-Link element.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_bvmlie_ext_mld_cap_op_info(uint8_t *mlie_seq, qdf_size_t mlie_seqlen,
				    bool *ext_mld_cap_found,
				    uint16_t *ext_mld_cap);

/**
 * util_get_bvmlie_persta_partner_info() - Get per-STA partner link information
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @partner_info: Pointer to the location where the partner link information
 * should be updated. This should be ignored by the caller if the function
 * returns error. Note that success will be returned and the number of links in
 * this structure will be reported as 0, if no Link Info is found, or no per-STA
 * profile is found, or if none of the per-STA profiles includes a MAC address
 * in the STA Info field (assuming no errors are encountered).
 *
 * Get partner link information and NSTR capability information in the
 * per-STA profiles present in a Basic variant Multi-Link element.
 * The partner link information is returned only for those per-STA profiles
 * which have a MAC address in the STA Info field.
 * The NSTR capability information is returned only for those per-STA profiles
 * which are Complete per-STA profiles.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_bvmlie_persta_partner_info(uint8_t *mlieseq,
				    qdf_size_t mlieseqlen,
				    struct mlo_partner_info *partner_info);

/**
 * util_get_prvmlie_mldid - Get the MLD ID from a given Probe Request
 * variant Multi-Link element , of the STA that transmits ML Probe Request
 * with the Multi-Link element
 *
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mldidfound: Pointer to the location where a boolean status should be
 * updated indicating whether the MLD ID was found or not. This should
 * be ignored by the caller if the function returns error.
 * @mldid: Pointer to the location where the value of the MLD ID
 * should be updated. This should be ignored by the caller if the function
 * returns error, or if the function indicates that the MLD ID was not
 * found.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_prvmlie_mldid(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		       bool *mldidfound, uint8_t *mldid);

/**
 * util_get_prvmlie_persta_link_id() - Get per-STA probe req link information
 *
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @probereq_info: Pointer to the location where the probe req link information
 * should be updated. This should be ignored by the caller if the function
 * returns error. Note that success will be returned and the number of links in
 * this structure will be reported as 0, if no Link Info is found, or no per-STA
 * profile is found.
 *
 * Get probe req link information in the per-STA profiles present in a Probe req
 * variant Multi-Link element.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_prvmlie_persta_link_id(uint8_t *mlieseq,
				qdf_size_t mlieseqlen,
				struct mlo_probereq_info *probereq_info);

/**
 * util_get_rvmlie_mldmacaddr() - Get the MLD MAC address from a given Reconfig
 * variant Multi-Link element.
 *
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mldmacaddr: Pointer to the location where the MLD MAC address should be
 * updated. This should be ignored by the caller if the function returns error.
 * @is_mldmacaddr_found: mld address found or not
 *
 * Get the MLD MAC address from a given Reconfig variant Multi-Link element
 * or element fragment sequence.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
util_get_rvmlie_mldmacaddr(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			   struct qdf_mac_addr *mldmacaddr,
			   bool *is_mldmacaddr_found);

/**
 * util_get_rvmlie_persta_link_info() - Get per-STA reconfig link information
 *
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @reconfig_info: Pointer to the location where the reconfig link information
 * should be updated. This should be ignored by the caller if the function
 * returns error. Note that success will be returned and the number of links in
 * this structure will be reported as 0, if no Link Info is found, or no per-STA
 * profile is found.
 *
 * Get reconfig link information in the per-STA profiles present in a Reconfig
 * variant Multi-Link element.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure.
 */
QDF_STATUS
util_get_rvmlie_persta_link_info(uint8_t *mlieseq,
				 qdf_size_t mlieseqlen,
				 struct ml_rv_info *reconfig_info);

/**
 * util_get_pav_mlie_link_info() - Get priority access link information
 *
 * @mlieseq: Starting address of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @mlieseqlen: Total length of the Multi-Link element or Multi-Link element
 * fragment sequence
 * @pa_info: Pointer to the location where the priority access multi link
 * information is stored.
 *
 * Get EPCS priority access information from Priority Access Multi-Link element.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure.
 */
QDF_STATUS util_get_pav_mlie_link_info(uint8_t *mlieseq,
				       qdf_size_t mlieseqlen,
				       struct ml_pa_info *pa_info);
#else
static inline QDF_STATUS
util_gen_link_assoc_req(uint8_t *frame, qdf_size_t frame_len, bool isreassoc,
			uint8_t link_id,
			struct qdf_mac_addr link_addr,
			uint8_t *link_frame,
			qdf_size_t link_frame_maxsize,
			qdf_size_t *link_frame_len)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_gen_link_assoc_rsp(uint8_t *frame, qdf_size_t frame_len, bool isreassoc,
			uint8_t link_id,
			struct qdf_mac_addr link_addr,
			uint8_t *link_frame,
			qdf_size_t link_frame_maxsize,
			qdf_size_t *link_frame_len)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_find_mlie(uint8_t *buf, qdf_size_t buflen, uint8_t **mlieseq,
	       qdf_size_t *mlieseqlen)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_find_mlie_by_variant(uint8_t *buf, qdf_size_t buflen, uint8_t **mlieseq,
			  qdf_size_t *mlieseqlen)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
util_get_mlie_variant(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		      int *variant)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_mlie_common_info_len(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			      uint8_t *commoninfo_len)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_bvmlie_bssparamchangecnt(uint8_t *mlieseq, qdf_size_t mlieseqlen,
				  bool *bssparamchangecntfound,
				  uint8_t *bssparamchangecnt)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_bvmlie_mldmacaddr(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			   struct qdf_mac_addr *mldmacaddr)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_bvmlie_eml_cap(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			bool *eml_cap_found,
			uint16_t *eml_cap)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_bvmlie_msd_cap(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			bool *msd_cap_found,
			uint16_t *msd_cap)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_bvmlie_primary_linkid(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			       bool *linkidfound, uint8_t *linkid)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_bvmlie_persta_partner_info(uint8_t *mlieseq,
				    qdf_size_t mlieseqlen,
				    struct mlo_partner_info *partner_info)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_prvmlie_persta_link_id(uint8_t *mlieseq,
				qdf_size_t mlieseqlen,
				struct mlo_probereq_info *probereq_info)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_prvmlie_mldid(uint8_t *mlieseq, qdf_size_t mlieseqlen,
		       bool *mldcapfound, uint8_t *mldcap)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_rvmlie_mldmacaddr(uint8_t *mlieseq, qdf_size_t mlieseqlen,
			   struct qdf_mac_addr *mldmacaddr,
			   bool *is_mldmacaddr_found)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
util_get_rvmlie_persta_link_info(uint8_t *mlieseq,
				 qdf_size_t mlieseqlen,
				 struct ml_rv_info *reconfig_info)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline
QDF_STATUS util_get_pav_mlie_link_info(uint8_t *mlieseq,
				       qdf_size_t mlieseqlen,
				       struct ml_pa_info *pa_info)
{
	return QDF_STATUS_E_NOSUPPORT;
}

#endif /* WLAN_FEATURE_11BE_MLO */
#endif /* _WLAN_UTILS_MLO_H_ */

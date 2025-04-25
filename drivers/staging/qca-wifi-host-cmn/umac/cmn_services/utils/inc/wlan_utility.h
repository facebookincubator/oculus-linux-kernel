/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Contains mandatory API from legacy
 */

#ifndef _WLAN_UTILITY_H_
#define _WLAN_UTILITY_H_

#include <qdf_types.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>

#define TGT_INVALID_SNR         (0)
#define TGT_MAX_SNR             (TGT_NOISE_FLOOR_DBM * (-1))
#define TGT_NOISE_FLOOR_DBM     (-96)
#define TGT_IS_VALID_SNR(x)     ((x) >= 0 && (x) < TGT_MAX_SNR)
#define TGT_IS_VALID_RSSI(x)    ((x) != 0xFF)

/**
 * struct wlan_vdev_ch_check_filter - vdev chan check filter object
 * @flag:     matches or not
 * @vdev:     vdev to be checked against all the active vdevs
 */
struct wlan_vdev_ch_check_filter {
	uint8_t flag;
	struct wlan_objmgr_vdev *vdev;
};

/**
 * struct wlan_op_mode_peer_count - vdev connected peer count
 * @opmode: QDF mode
 * @peer_count: peer count
 **/
struct wlan_op_mode_peer_count {
	enum QDF_OPMODE opmode;
	uint16_t peer_count;
};

/**
 * wlan_construct_shortssid() - construct the short ssid with the help of
 * shortssid table
 * @ssid: pointer to ssid
 * @ssid_len: ssid length
 *
 * return: short ssid length
 */
uint32_t wlan_construct_shortssid(uint8_t *ssid, uint8_t ssid_len);

/**
 * wlan_chan_to_freq() - converts channel to frequency
 * @chan: channel number
 *
 * @return frequency of the channel
 */
uint32_t wlan_chan_to_freq(uint8_t chan);

/**
  * wlan_get_320_center_freq() - find center frequencies for 320Mhz channel
  * @freq: Primary frequency
  * @center_freq1: possible 1st center frequency
  * @center_freq2: possible 2nd center frequency
  *
  * return: void
  **/
void
wlan_get_320_center_freq(qdf_freq_t freq,
			 qdf_freq_t *center_freq1,
			 qdf_freq_t *center_freq2);

/**
 * wlan_freq_to_chan() - converts frequency to channel
 * @freq: frequency
 *
 * Return: channel of frequency
 */
uint8_t wlan_freq_to_chan(uint32_t freq);

/**
 * wlan_is_ie_valid() - Determine if an IE sequence is valid
 * @ie: Pointer to the IE buffer
 * @ie_len: Length of the IE buffer @ie
 *
 * This function validates that the IE sequence is valid by verifying
 * that the sum of the lengths of the embedded elements match the
 * length of the sequence.
 *
 * Note well that a 0-length IE sequence is considered valid.
 *
 * Return: true if the IE sequence is valid, false if it is invalid
 */
bool wlan_is_ie_valid(const uint8_t *ie, size_t ie_len);

/**
 * wlan_get_ie_ptr_from_eid() - Find out ie from eid
 * @eid: element id
 * @ie: source ie address
 * @ie_len: source ie length
 *
 * Return: vendor ie address - success
 *         NULL - failure
 */
const uint8_t *wlan_get_ie_ptr_from_eid(uint8_t eid,
					const uint8_t *ie,
					int ie_len);

/**
 * wlan_get_vendor_ie_ptr_from_oui() - Find out vendor ie
 * @oui: oui buffer
 * @oui_size: oui size
 * @ie: source ie address
 * @ie_len: source ie length
 *
 * This function find out vendor ie by pass source ie and vendor oui.
 *
 * Return: vendor ie address - success
 *         NULL - failure
 */
const uint8_t *wlan_get_vendor_ie_ptr_from_oui(const uint8_t *oui,
					       uint8_t oui_size,
					       const uint8_t *ie,
					       uint16_t ie_len);

/**
 * wlan_get_ext_ie_ptr_from_ext_id() - Find out ext ie
 * @oui: oui buffer
 * @oui_size: oui size
 * @ie: source ie address
 * @ie_len: source ie length
 *
 * This function find out ext ie from ext id (passed oui)
 *
 * Return: vendor ie address - success
 *         NULL - failure
 */
const uint8_t *wlan_get_ext_ie_ptr_from_ext_id(const uint8_t *oui,
					       uint8_t oui_size,
					       const uint8_t *ie,
					       uint16_t ie_len);

/**
 * wlan_iecap_set() - Set the capability in the IE
 * @iecap: pointer to capability IE
 * @bit_pos: bit position of capability from start of capability field
 * @tot_bits: total bits of capability
 * @value: value to be set
 *
 * This function sets the value in capability IE at the bit position
 * specified for specified number of bits in byte order.
 *
 * Return: void
 */
void wlan_iecap_set(uint8_t *iecap,
		    uint8_t bit_pos,
		    uint8_t tot_bits,
		    uint32_t value);

/**
 * wlan_iecap_get() - Get the capability in the IE
 * @iecap: pointer to capability IE
 * @bit_pos: bit position of capability from start of capability field
 * @tot_bits: total bits of capability
 *
 * This function gets the value at bit position for specified bits
 * from start of capability field.
 *
 * Return: capability value
 */
uint32_t wlan_iecap_get(uint8_t *iecap,
			uint8_t bit_pos,
			uint32_t tot_bits);

/**
 * wlan_get_elem_fragseq_requirements() - Get requirements related to generation
 * of element fragment sequence.
 *
 * @elemid: Element ID
 * @payloadlen: Length of element payload to be fragmented. Irrespective of
 * whether inline fragmentation in wlan_create_elem_fragseq() is to be used or
 * not, this length should not include the length of the element ID and element
 * length, and if the element ID is WLAN_ELEMID_EXTN_ELEM, it should not include
 * the length of the element ID extension.
 * @is_frag_required: Pointer to location where the function should update
 * whether fragmentation is required or not for the given element ID and payload
 * length. The caller should ignore this if the function returns failure.
 * @required_fragbuff_size: Pointer to location where the function should update
 * the required minimum size of the buffer where the fragment sequence created
 * would be written, starting from the beginning of the buffer (irrespective of
 * whether inline fragmentation in wlan_create_elem_fragseq() is to be used or
 * not). This is the total size of the element fragment sequence, inclusive of
 * the header and payload of the leading element and the headers and payloads of
 * all subsequent fragments applicable to that element. If the element ID is
 * WLAN_ELEMID_EXTN_ELEM, this also includes the length of the element ID
 * extension. The caller should ignore this if the function returns a value of
 * false for is_frag_required, or if the function returns failure.
 *
 * Get information on requirements related to generation of element fragment
 * sequence. Currently this includes an indication of whether fragmentation is
 * required or not for the given element ID and payload length, and if
 * fragmentation is applicable, the minimum required size of the buffer where
 * the fragment sequence created would be written (irrespective of whether
 * inline fragmentation in wlan_create_elem_fragseq() is to be used or not).
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
wlan_get_elem_fragseq_requirements(uint8_t elemid,
				   qdf_size_t payloadlen,
				   bool *is_frag_required,
				   qdf_size_t *required_fragbuff_size);

/**
 * wlan_create_elem_fragseq() - Create sequence of element fragments
 *
 * @inline_frag: Whether to use inline fragmentation, wherein the fragmentation
 * is carried out inline within the source buffer and no memmoves/memcopy would
 * be required for the lead element.
 * @elemid: Element ID
 * @elemidext: Element ID extension. This is applicable only if elemid is
 * WLAN_ELEMID_EXTN_ELEM, otherwise it is ignored.
 * @payloadbuff: Buffer containing the element payload to be fragmented. If
 * inline fragmentation is selected, the corresponding element fragment sequence
 * will be generated inline into this buffer, and prior to the payload the
 * buffer should have two bytes reserved in the beginning for the element ID and
 * element length fields to be written, and a third byte reserved after them for
 * the element ID extension to be written (if the element ID is
 * WLAN_ELEMID_EXTN_ELEM).
 * @payloadbuff_maxsize: Maximum size of payloadbuff
 * @payloadlen: Length of element payload to be fragmented. Irrespective of
 * whether inline fragmentation is to be used or not, this should not include
 * the length of the element ID and element length, and if the element ID is
 * WLAN_ELEMID_EXTN_ELEM, it should not include the length of the element ID
 * extension.
 * @fragbuff: The buffer into which the element fragment sequence should be
 * generated. This is inapplicable and ignored if inline fragmentation is used.
 * @fragbuff_maxsize: The maximum size of fragbuff. This is inapplicable and
 * ignored if inline fragmentation is used.
 * @fragseqlen: Pointer to location where the length of the fragment sequence
 * created should be written. This is the total length of the element fragment
 * sequence, inclusive of the header and payload of the leading element and the
 * headers and payloads of all subsequent fragments applicable to that element.
 * If the element ID is WLAN_ELEMID_EXTN_ELEM, this also includes the length of
 * the element ID extension. The caller should ignore this if the function
 * returns failure.
 *
 * Create a sequence of element fragments. In case fragmentation is not required
 * for the given element ID and payload length, the function returns an error.
 * This function is intended to be used by callers which do not have the ability
 * (or for maintainability purposes do not desire the complexity) to inject new
 * fragments on the fly where required, when populating the fields in the
 * element (which would completely eliminate memory moves/copies). An inline
 * mode is available to carry out the fragmentation within the source buffer in
 * order to reduce buffer requirements and to eliminate memory copies/moves for
 * the lead element. In the inline mode, the source buffer should have bytes
 * reserved in the beginning for the element ID, element length, and if
 * applicable, the element ID extension. In the inline mode the buffer content
 * (if any) after the fragments is moved as well.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS wlan_create_elem_fragseq(bool inline_frag,
				    uint8_t elemid,
				    uint8_t elemidext,
				    uint8_t *payloadbuff,
				    qdf_size_t payloadbuff_maxsize,
				    qdf_size_t payloadlen,
				    uint8_t *fragbuff,
				    qdf_size_t fragbuff_maxsize,
				    qdf_size_t *fragseqlen);

/**
 * wlan_get_subelem_fragseq_requirements() - Get requirements related to
 * generation of subelement fragment sequence.
 *
 * @subelemid: Subelement ID
 * @payloadlen: Length of subelement payload to be fragmented. Irrespective of
 * whether inline fragmentation in wlan_create_subelem_fragseq() is to be used
 * or not, this length should not include the length of the subelement ID and
 * subelement length.
 * @is_frag_required: Pointer to location where the function should update
 * whether fragmentation is required or not for the given payload length. The
 * caller should ignore this if the function returns failure.
 * @required_fragbuff_size: Pointer to location where the function should update
 * the required minimum size of the buffer where the fragment sequence created
 * would be written, starting from the beginning of the buffer (irrespective of
 * whether inline fragmentation in wlan_create_subelem_fragseq() is to be used
 * or not). This is the total size of the subelement fragment sequence,
 * inclusive of the header and payload of the leading subelement and the headers
 * and payloads of all subsequent fragments applicable to that subelement. The
 * caller should ignore this if the function returns a value of false for
 * is_frag_required, or if the function returns failure.
 *
 * Get information on requirements related to generation of subelement fragment
 * sequence. Currently this includes an indication of whether fragmentation is
 * required or not for the given payload length, and if fragmentation is
 * applicable, the minimum required size of the buffer where the fragment
 * sequence created would be written (irrespective of whether inline
 * fragmentation in wlan_create_subelem_fragseq() is to be used or not). Note
 * that the subelement ID does not currently play a role in determining the
 * requirements, but is added as an argument in case it is required in the
 * future.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS
wlan_get_subelem_fragseq_requirements(uint8_t subelemid,
				      qdf_size_t payloadlen,
				      bool *is_frag_required,
				      qdf_size_t *required_fragbuff_size);

/**
 * wlan_create_subelem_fragseq() - Create sequence of subelement fragments
 *
 * @inline_frag: Whether to use inline fragmentation, wherein the fragmentation
 * is carried out inline within the source buffer and no memmoves/memcopy would
 * be required for the lead subelement.
 * @subelemid: Subelement ID
 * @subelemfragid: Fragment ID to be used for the subelement (this can
 * potentially vary across protocol areas)
 * @payloadbuff: Buffer containing the subelement payload to be fragmented. If
 * inline fragmentation is selected, the corresponding subelement fragment
 * sequence will be generated inline into this buffer, and prior to the payload
 * the buffer should have two bytes reserved in the beginning for the subelement
 * ID and subelement length fields to be written.
 * @payloadbuff_maxsize: Maximum size of payloadbuff
 * @payloadlen: Length of subelement payload to be fragmented. Irrespective of
 * whether inline fragmentation is to be used or not, this should not include
 * the length of the subelement ID and subelement length.
 * @fragbuff: The buffer into which the subelement fragment sequence should be
 * generated. This is inapplicable and ignored if inline fragmentation is used.
 * @fragbuff_maxsize: The maximum size of fragbuff. This is inapplicable and
 * ignored if inline fragmentation is used.
 * @fragseqlen: Pointer to location where the length of the fragment sequence
 * created should be written. This is the total length of the subelement
 * fragment sequence, inclusive of the header and payload of the leading
 * subelement and the headers and payloads of all subsequent fragments
 * applicable to that subelement. The caller should ignore this if the function
 * returns failure.
 *
 * Create a sequence of subelement fragments. In case fragmentation is not
 * required for the given payload length, the function returns an error. This
 * function is intended to be used by callers which do not have the ability (or
 * for maintainability purposes do not desire the complexity) to inject new
 * fragments on the fly where required, when populating the fields in the
 * subelement (which would completely eliminate memory moves/copies). An inline
 * mode is available to carry out the fragmentation within the source buffer in
 * order to reduce buffer requirements and to eliminate memory copies/moves for
 * the lead subelement. In the inline mode, the source buffer should have bytes
 * reserved in the beginning for the subelement ID and the subelement length. In
 * the inline mode the buffer content (if any) after the fragments is moved as
 * well.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS wlan_create_subelem_fragseq(bool inline_frag,
				       uint8_t subelemid,
				       uint8_t subelemfragid,
				       uint8_t *payloadbuff,
				       qdf_size_t payloadbuff_maxsize,
				       qdf_size_t payloadlen,
				       uint8_t *fragbuff,
				       qdf_size_t fragbuff_maxsize,
				       qdf_size_t *fragseqlen);

/**
 * wlan_get_elem_fragseq_info() - Get information about element fragment
 * sequence
 *
 * @elembuff: Buffer containing a series of elements to be checked for whether a
 * contiguous subset of these elements (starting with the first element in the
 * buffer) form an element fragment sequence. The buffer should start with the
 * Element ID of the first element. The buffer should not contain any material
 * other than elements.
 * @elembuff_maxsize: Maximum size of elembuff
 * @is_fragseq: Pointer to location of a flag indicating whether this is an
 * element fragment sequence or not. The flag will be set to true if elembuff
 * contains an element fragment sequence starting with the element present in
 * the beginning of the buffer, or the flag will be set to false if the buffer
 * contains a single non-fragmented element in the beginning. Please note
 * standards related limitation given in function description below.
 * @fragseq_totallen: Pointer to location of total length of element fragment
 * sequence. If is_fragseq is true, then this is set to the total length of the
 * element fragment sequence, inclusive of the header and payload of the leading
 * element and the headers and payloads of all subsequent fragments applicable
 * to that element. If is_fragseq is false, the caller should ignore this.
 * Please note standards related limitation given in function description below.
 * @fragseq_payloadlen: Pointer to location of length of payload of element
 * fragment sequence. If is_fragseq is true, then this length is set to the
 * total size of the element fragment sequence payload, which does not include
 * the sizes of the headers of the lead element and subsequent fragments, and
 * which (if the lead element's element ID is WLAN_ELEMID_EXTN_ELEM) does not
 * include the size of the lead element's element ID extension. If is_fragseq is
 * false, the caller should ignore this. Please note standards related
 * limitation given in function description below.
 *
 * Get the following information for a first element present in the beginning of
 * a given buffer, and a series of elements after it in the given buffer: a)
 * Whether a contiguous subset of these elements starting with the first element
 * form an element fragment sequence. b) If they form an element fragment
 * sequence, then the total length of this sequence inclusive of headers and
 * payloads of all the elements in the sequence. c) If they form an element
 * fragment sequence, then the total size of the payloads of all the elements in
 * the sequence (not including the element ID extension of the lead element, if
 * applicable). While determining this information, the function may return
 * errors, including for protocol parsing issues. These protocol parsing issues
 * include one in which the first element has a length lesser than 255, but the
 * very next element after it is a fragment element (which is not allowed by the
 * standard).  Separately, please note a limitation arising from the standard
 * wherein if the caller passes a truncated maximum buffer size such that the
 * buffer ends prematurely just at the end of a potential lead element with
 * length 255 or just at the end of a non-lead fragment element with length 255,
 * the function will have to conclude that the last successfully parsed element
 * is the final one in the non-fragment or fragment sequence, and return results
 * accordingly. If another fragment actually exists beyond the given buffer,
 * this function cannot detect the condition since there is no provision in the
 * standard to indicate a total fragment sequence size in one place in the
 * beginning or anywhere else. Hence the caller should take care to provide the
 * complete buffer with the max size set accordingly.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS wlan_get_elem_fragseq_info(uint8_t *elembuff,
				      qdf_size_t elembuff_maxsize,
				      bool *is_fragseq,
				      qdf_size_t *fragseq_totallen,
				      qdf_size_t *fragseq_payloadlen);

/**
 * wlan_defrag_elem_fragseq() - Defragment sequence of element fragments
 *
 * @inline_defrag: Whether to use inline defragmentation, wherein the
 * defragmentation is carried out inline within the source buffer and no
 * memmoves/memcopy would be required for the lead element.
 * @fragbuff: Source buffer containing the element fragment sequence starting
 * with the Element ID of the lead element. The buffer should not contain any
 * material other than elements. If inline defragmentation is enabled, the
 * corresponding defragmented payload will be generated inline into this buffer
 * and the defragmented payload will start after the location of the lead
 * element's element ID, element length, and (if the lead element's element ID
 * is WLAN_ELEMID_EXTN_ELEM), the element ID extension. This defragmented
 * payload will not contain the headers of any of the other fragments in the
 * fragment sequence.
 * @fragbuff_maxsize: Maximum size of fragbuff. This should be greater than or
 * equal to the total size of the element fragment sequence, inclusive of the
 * header and payload of the leading element and the headers and payloads of all
 * subsequent fragments applicable to that element.
 * @defragbuff: The destination buffer into which the defragmented payload
 * should be copied. This is inapplicable and ignored if inline_defrag is true.
 * The defragmented payload will be copied to the start of the destination
 * buffer without including the headers of the lead element and the subsequent
 * fragment elements, and (if the lead element's element ID is
 * WLAN_ELEMID_EXTN_ELEM), without including the element ID extension.
 * @defragbuff_maxsize: Maximum size of defragbuff. This is inapplicable and
 * ignored if inline_defrag is true. The size should be large enough to contain
 * the entire defragmented payload, otherwise an error will be returned.
 * @defragpayload_len: Pointer to the location where the length of the
 * defragmented payload should be updated. Irrespective of whether inline_defrag
 * is true or false, this will not include the sizes of the headers of the lead
 * element and subsequent fragments, and (if the lead element's element ID is
 * WLAN_ELEMID_EXTN_ELEM), it will not include the size of the lead element's
 * element ID extension. Please note standards related limitation given in
 * function description below.
 *
 * Defragment a sequence of element fragments. If the source buffer does not
 * contain an element fragment sequence (in the beginning), an error is
 * returned. An inline mode is available to carry out the defragmentation within
 * the source buffer in order to reduce buffer requirements and to eliminate
 * memory copies/moves for the lead element. In the inline mode, the buffer
 * content (if any) after the fragments is moved as well. The contents of the
 * defragmented payload are intended for end consumption by control path
 * protocol processing code within the driver in a manner uniform with other
 * protocol data in byte buffers, and not for onward forwarding to other
 * subsystems or for intrusive specialized processing different from other
 * protocol data. Hence zero copy methods such as network buffer fragment
 * processing, etc. are not used in this use case.  Additionally, this API is
 * intended for use cases where the nature of the payload is complex and it is
 * infeasible for the caller to skip the (un-defragmented) fragment boundaries
 * on its own in a scalable and maintainable manner. Separately, please note a
 * limitation arising from the standard wherein if the caller passes a truncated
 * maximum buffer size such that the buffer ends prematurely just at the end of
 * a fragment element with length 255, the function will have to conclude that
 * the last successfully parsed fragment element is the final one in the
 * fragment sequence, and return results accordingly. If another fragment
 * actually exists beyond the given buffer, this function cannot detect the
 * condition since there is no provision in the standard to indicate a total
 * fragment sequence size in one place in the beginning or anywhere else. Hence
 * the caller should take care to provide the complete buffer with the max size
 * set accordingly.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS wlan_defrag_elem_fragseq(bool inline_defrag,
				    uint8_t *fragbuff,
				    qdf_size_t fragbuff_maxsize,
				    uint8_t *defragbuff,
				    qdf_size_t defragbuff_maxsize,
				    qdf_size_t *defragpayload_len);

/**
 * wlan_get_subelem_fragseq_info() - Get information about subelement fragment
 * sequence
 *
 * @subelemfragid: Fragment ID applicable for the subelement (this can
 * potentially vary across protocol areas)
 * @subelembuff: Buffer containing a series of subelements to be checked for
 * whether a contiguous subset of these subelements (starting with the first
 * subelement in the buffer) form a subelement fragment sequence. The containing
 * element is required to have already been defragmented (if applicable). The
 * buffer should start with the subelement ID of the first subelement. The
 * buffer should not contain any material apart from subelements.
 * @subelembuff_maxsize: Maximum size of subelembuff
 * @is_fragseq: Pointer to location of a flag indicating whether this is a
 * subelement fragment sequence or not. The flag will be set to true if the
 * buffer contains a subelement fragment sequence starting with the subelement
 * present in the beginning of the buffer, or the flag will be set to false if
 * the buffer contains a single non-fragmented subelement in the beginning.
 * Please note standards related limitation given in function description below.
 * @fragseq_totallen: Pointer to location of total length of subelement fragment
 * sequence. If is_fragseq is true, then this is set to the total length of the
 * subelement fragment sequence, inclusive of the header and payload of the
 * leading subelement and the headers and payloads of all subsequent fragments
 * applicable to that subelement. If is_fragseq is false, the caller should
 * ignore this. Please note standards related limitation given in function
 * description below.
 * @fragseq_payloadlen: Pointer to location of length of payload of subelement
 * fragment sequence. If is_fragseq is true, then this length is set to the
 * total size of the subelement fragment sequence payload, which does not
 * include the sizes of the headers of the lead subelement and subsequent
 * fragments. If is_fragseq is false, the caller should ignore this. Please note
 * standards related limitation given in function description below.
 *
 * Get the following information for a first subelement present in the beginning
 * of a given buffer, and a series of subelements after it in the given buffer:
 * a) Whether a contiguous subset of these subelements starting with the first
 * subelement form a subelement fragment sequence. b) If they form a subelement
 * fragment sequence, then the total length of this sequence inclusive of
 * headers and payloads of all the subelements in the sequence. c) If they form
 * a subelement fragment sequence, then the total size of the payloads of all
 * the subelements in the sequence.  While determining this information, the
 * function may return errors, including for protocol parsing issues. These
 * protocol parsing issues include one in which the first subelement has a
 * length lesser than 255, but the very next subelement after it is a fragment
 * subelement (which is not allowed by the standard so far). Separately, please
 * note a limitation arising from the standard wherein if the caller passes a
 * truncated maximum buffer size such that the buffer ends prematurely just at
 * the end of a potential lead subelement with length 255 or just at the end of
 * a non-lead fragment subelement with length 255, the function will have to
 * conclude that the last successfully parsed subelement is the final one in the
 * non-fragment or fragment sequence, and return results accordingly. If another
 * fragment actually exists beyond the given buffer, this function cannot detect
 * the condition since there is no provision in the standard to indicate a total
 * fragment sequence size in one place in the beginning or anywhere else. Hence
 * the caller should take care to provide the complete buffer with the max size
 * set accordingly.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS wlan_get_subelem_fragseq_info(uint8_t subelemfragid,
					 uint8_t *subelembuff,
					 qdf_size_t subelembuff_maxsize,
					 bool *is_fragseq,
					 qdf_size_t *fragseq_totallen,
					 qdf_size_t *fragseq_payloadlen);

/**
 * wlan_defrag_subelem_fragseq() - Defragment sequence of subelement fragments
 *
 * @inline_defrag: Whether to use inline defragmentation, wherein the
 * defragmentation is carried out inline within the source buffer and no
 * memmoves/memcopy would be required for the lead subelement.
 * @subelemfragid: Fragment ID applicable for the subelement (this can
 * potentially vary across protocol areas)
 * @fragbuff: Source buffer containing the subelement fragment sequence starting
 * with the subelement ID of the lead subelement. The containing element is
 * required to have already been defragmented (if applicable). If inline
 * defragmentation is enabled, the corresponding defragmented payload will be
 * generated inline into this buffer and the defragmented payload will start
 * after the location of the lead subelement's subelement ID and subelement
 * length. This defragmented payload will not contain the headers of any of the
 * other fragments in the fragment sequence.
 * @fragbuff_maxsize: Maximum size of fragbuff. This should be greater than or
 * equal to the total size of the subelement fragment sequence, inclusive of the
 * header and payload of the leading subelement and the headers and payloads of
 * all subsequent fragments applicable to that subelement.
 * @defragbuff: The destination buffer into which the defragmented payload
 * should be copied. This is inapplicable and ignored if inline_defrag is true.
 * The defragmented payload will be copied to the start of the destination
 * buffer without including the headers of the lead subelement and the
 * subsequent fragment subelements.
 * @defragbuff_maxsize: Maximum size of defragbuff. This is inapplicable and
 * ignored if inline_defrag is true. The size should be large enough to contain
 * the entire defragmented payload, otherwise an error will be returned.
 * @defragpayload_len: Pointer to the location where the length of the
 * defragmented payload should be updated. Irrespective of whether inline_defrag
 * is true or false, this will not include the sizes of the headers of the lead
 * subelement and subsequent fragments. Please note standards related limitation
 * given in function description below.
 *
 * Defragment a sequence of subelement fragments. If the source buffer does not
 * contain a subelement fragment sequence (in the beginning), the function
 * returns an error. The containing element is required to have already been
 * defragmented. An inline mode is available to carry out the defragmentation
 * within the source buffer in order to reduce buffer requirements and to
 * eliminate memory copies/moves for the lead subelement. In the inline mode,
 * the buffer content (if any) after the fragments is moved as well. The
 * contents of the defragmented payload are intended for end consumption by
 * control path protocol processing code within the driver in a manner uniform
 * with other protocol data in byte buffers, and not for onward forwarding to
 * other subsystems or for intrusive specialized processing different from other
 * protocol data. Hence zero copy methods such as network buffer fragment
 * processing, etc. are not used in this use case.  Additionally, this API is
 * intended for use cases where the nature of the payload is complex and it is
 * infeasible for the caller to skip the (un-defragmented) fragment boundaries
 * on its own in a scalable and maintainable manner.  Separately, please note a
 * limitation arising from the standard wherein if the caller passes a truncated
 * maximum buffer size such that the buffer ends prematurely just at the end of
 * a fragment subelement with length 255, the function will have to conclude
 * that the last successfully parsed fragment subelement is the final one in the
 * fragment sequence, and return results accordingly. If another fragment
 * actually exists beyond the given buffer, this function cannot detect the
 * condition since there is no provision in the standard to indicate a total
 * fragment sequence size in one place in the beginning or anywhere else. Hence
 * the caller should take care to provide the complete buffer with the max size
 * set accordingly.
 *
 * Return: QDF_STATUS_SUCCESS in the case of success, QDF_STATUS value giving
 * the reason for error in the case of failure
 */
QDF_STATUS wlan_defrag_subelem_fragseq(bool inline_defrag,
				       uint8_t subelemfragid,
				       uint8_t *fragbuff,
				       qdf_size_t fragbuff_maxsize,
				       uint8_t *defragbuff,
				       qdf_size_t defragbuff_maxsize,
				       qdf_size_t *defragpayload_len);

/**
 * wlan_is_emulation_platform() - check if platform is emulation based
 * @phy_version: psoc nif phy_version
 *
 * Return: boolean value based on platform type
 */
bool wlan_is_emulation_platform(uint32_t phy_version);

/**
 * wlan_get_pdev_id_from_vdev_id() - Helper func to derive pdev id from vdev_id
 * @psoc: psoc object
 * @vdev_id: vdev identifier
 * @dbg_id: object manager debug id
 *
 * This function is used to derive the pdev id from vdev id for a psoc
 *
 * Return : pdev_id - +ve integer for success and WLAN_INVALID_PDEV_ID
 *          for failure
 */
uint32_t wlan_get_pdev_id_from_vdev_id(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id,
				 wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_util_is_vdev_active() - Check for vdev active
 * @pdev: pdev pointer
 * @dbg_id: debug id for ref counting
 *
 * Return: QDF_STATUS_SUCCESS in case of vdev active
 *          QDF_STATUS_E_INVAL, if dev is not active
 */
QDF_STATUS wlan_util_is_vdev_active(struct wlan_objmgr_pdev *pdev,
				    wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_vdev_is_up() - Check for vdev is in UP state
 * @vdev: vdev pointer
 *
 * Return: QDF_STATUS_SUCCESS, if vdev is in up, otherwise QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_vdev_is_up(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_util_pdev_vdevs_deschan_match() - function to check des channel matches
 *                                        with other vdevs in pdev
 * @pdev: pdev object
 * @vdev: vdev object
 * @dbg_id: object manager ref id
 *
 * This function checks the vdev desired channel with other vdev channels
 *
 * Return: QDF_STATUS_SUCCESS, if it matches, otherwise QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_util_pdev_vdevs_deschan_match(struct wlan_objmgr_pdev *pdev,
					      struct wlan_objmgr_vdev *vdev,
					      wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_util_change_map_index() - function to set/reset given index bit
 * @map: bitmpap
 * @id: bit index
 * @set: 1 for set, 0 of reset
 *
 * This function set/reset given index bit
 *
 * Return: void
 */
void wlan_util_change_map_index(unsigned long *map, uint8_t id, uint8_t set);

/**
 * wlan_util_map_index_is_set() - function to check whether given index bit is
 *                                set
 * @map: bitmpap
 * @id: bit index
 *
 * This function checks the given index bit is set
 *
 * Return: true, if bit is set, otherwise false
 */
bool wlan_util_map_index_is_set(unsigned long *map, uint8_t id);

/**
 * wlan_util_map_is_any_index_set() - Check if any bit is set in given bitmap
 * @map: bitmap
 * @nbytes: number of bytes in bitmap
 *
 * Return: true, if any of the bit is set, otherwise false
 */
bool wlan_util_map_is_any_index_set(unsigned long *map, unsigned long nbytes);

/**
 * wlan_pdev_chan_change_pending_vdevs() - function to test/set channel change
 *                                         pending flag
 * @pdev: pdev object
 * @vdev_id_map: bitmap to derive channel change vdevs
 * @dbg_id: object manager ref id
 *
 * This function test/set channel change pending flag
 *
 * Return: QDF_STATUS_SUCCESS, if it iterates through all vdevs,
 *         otherwise QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_pdev_chan_change_pending_vdevs(struct wlan_objmgr_pdev *pdev,
					       unsigned long *vdev_id_map,
					       wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_pdev_chan_change_pending_vdevs_down() - function to test/set down
 *                                              change pending flag
 * @pdev: pdev object
 * @vdev_id_map: bitmap to derive channel change vdevs
 * @dbg_id: object manager ref id
 *
 * This function test/set channel change pending flag
 *
 * Return: QDF_STATUS_SUCCESS, if it iterates through all vdevs,
 *         otherwise QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_pdev_chan_change_pending_vdevs_down(
					struct wlan_objmgr_pdev *pdev,
					unsigned long *vdev_id_map,
					wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_pdev_chan_change_pending_ap_vdevs_down() - function to test/set channel
 *                                            change pending flag for AP VDEVs
 * @pdev: pdev object
 * @vdev_id_map: bitmap to derive channel change AP vdevs
 * @dbg_id: object manager ref id
 *
 * This function test/set channel change pending flag for AP vdevs
 *
 * Return: QDF_STATUS_SUCCESS, if it iterates through all vdevs,
 *         otherwise QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_pdev_chan_change_pending_ap_vdevs_down(
					struct wlan_objmgr_pdev *pdev,
					unsigned long *vdev_id_map,
					wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_chan_eq() - function to check whether both channels are same
 * @chan1: channel1 object
 * @chan2: channel2 object
 *
 * This function checks the chan1 and chan2 are same
 *
 * Return: QDF_STATUS_SUCCESS, if it matches, otherwise QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_chan_eq(struct wlan_channel *chan1, struct wlan_channel *chan2);

/**
 * wlan_chan_copy() - function to copy channel
 * @tgt:  target channel object
 * @src:  src achannel object
 *
 * This function copies channel data from src to tgt
 *
 * Return: void
 */
void wlan_chan_copy(struct wlan_channel *tgt, struct wlan_channel *src);

/**
 * wlan_vdev_get_active_channel() - derives the vdev operating channel
 * @vdev:  VDEV object
 *
 * This function checks vdev state and return the channel pointer accordingly
 *
 * Return: active channel, if vdev chan config is valid
 *         NULL, if VDEV is in INIT or STOP state
 */
struct wlan_channel *wlan_vdev_get_active_channel
				(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_get_connected_vdev_by_bssid() - check/get any vdev connected on bssid
 * @pdev: pdev object
 * @bssid: bssid to be checked
 * @vdev_id: vdev id
 *
 * This function will loop through all the vdev in psoc and find/return the
 * vdev which is connected to bssid provided.
 *
 * Return: bool
 */
bool wlan_get_connected_vdev_by_bssid(struct wlan_objmgr_pdev *pdev,
				      uint8_t *bssid, uint8_t *vdev_id);

/**
 * wlan_get_connected_vdev_from_psoc_by_bssid() - check/get any vdev
 *                                                connected on bssid
 * @psoc: psoc object
 * @bssid: bssid to be checked
 * @vdev_id: vdev id
 *
 * This function will loop through all the vdev in psoc and find/return the
 * vdev which is connected to bssid provided.
 *
 * Return: bool
 */
bool wlan_get_connected_vdev_from_psoc_by_bssid(struct wlan_objmgr_psoc *psoc,
						uint8_t *bssid,
						uint8_t *vdev_id);

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * wlan_get_connected_vdev_by_mld_addr() - check/get any vdev
 *                                         connected on mld mac
 * @psoc: psoc object
 * @mld_mac: mld mac to be checked
 * @vdev_id: vdev id
 *
 * This function will loop through all the vdev in psoc and find/return the
 * first vdev which is connected to mld mac provided.
 *
 * Return: bool
 */
bool wlan_get_connected_vdev_by_mld_addr(struct wlan_objmgr_psoc *psoc,
					 uint8_t *mld_mac, uint8_t *vdev_id);
#endif

/**
 * wlan_util_stats_get_rssi() - API to get rssi in dbm
 * @db2dbm_enabled: If db2dbm capability is enabled
 * @bcn_snr: beacon snr
 * @dat_snr: data snr
 * @rssi: rssi
 *
 * This function gets the rssi based on db2dbm support. If this feature is
 * present in hw then it means firmware directly sends rssi and no conversion
 * is required. If this capability is not present then host needs to convert
 * snr to rssi
 *
 * Return: None
 */
void
wlan_util_stats_get_rssi(bool db2dbm_enabled, int32_t bcn_snr, int32_t dat_snr,
			 int8_t *rssi);

/**
 * wlan_util_is_pdev_restart_progress() - Check if any vdev is in restart state
 * @pdev: pdev pointer
 * @dbg_id: module id
 *
 * Iterates through all vdevs, checks if any VDEV is in RESTART_PROGRESS
 * substate
 *
 * Return: QDF_STATUS_SUCCESS,if any vdev is in RESTART_PROGRESS substate
 *         otherwise QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_util_is_pdev_restart_progress(struct wlan_objmgr_pdev *pdev,
					      wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_util_is_pdev_scan_allowed() - Check for vdev is allowed to scan
 * @pdev: pdev pointer
 * @dbg_id: module id
 *
 * Iterates through all vdevs, checks if any VDEV is not either in S_INIT or in
 * S_UP state
 *
 * Return: QDF_STATUS_SUCCESS,if scan is allowed, otherwise QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_util_is_pdev_scan_allowed(struct wlan_objmgr_pdev *pdev,
					  wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_util_get_peer_count_for_mode - This api gives vdev mode specific
 * peer count`
 * @pdev: PDEV object
 * @mode: Operation mode.
 *
 * Return: int- peer count for operating mode
 */
uint16_t wlan_util_get_peer_count_for_mode(struct wlan_objmgr_pdev *pdev,
					   enum QDF_OPMODE mode);

/**
 * enum wlan_minidump_host_data - Data structure type logged in Minidump
 * @WLAN_MD_CP_EXT_PDEV: ol_ath_softc_net80211
 * @WLAN_MD_CP_EXT_PSOC: ol_ath_soc_softc
 * @WLAN_MD_CP_EXT_VDEV: ieee80211vap
 * @WLAN_MD_CP_EXT_PEER: ieee80211_node
 * @WLAN_MD_DP_SOC: dp_soc
 * @WLAN_MD_DP_PDEV: dp_pdev
 * @WLAN_MD_DP_VDEV: dp_vdev
 * @WLAN_MD_DP_PEER: dp_peer
 * @WLAN_MD_DP_SRNG_REO_DEST: dp_srng type for reo dest
 * @WLAN_MD_DP_SRNG_REO_EXCEPTION: dp_srng type for reo exception
 * @WLAN_MD_DP_SRNG_REO_CMD: dp_srng type for reo cmd
 * @WLAN_MD_DP_SRNG_RX_REL: dp_srng type for reo release
 * @WLAN_MD_DP_SRNG_REO_REINJECT: dp_srng type for reo reinject
 * @WLAN_MD_DP_SRNG_REO_STATUS: dp_srng type for reo status
 * @WLAN_MD_DP_SRNG_TCL_DATA: dp_srng type for tcl data
 * @WLAN_MD_DP_SRNG_TCL_CMD: dp_srng type for tcl cmd
 * @WLAN_MD_DP_SRNG_TCL_STATUS: dp_srng type for tcl status
 * @WLAN_MD_DP_SRNG_TX_COMP: dp_srng type for tcl comp
 * @WLAN_MD_DP_SRNG_WBM_DESC_REL: dp_srng_type for wbm desc rel
 * @WLAN_MD_DP_SRNG_WBM_IDLE_LINK: dp_srng type for wbm idle link
 * @WLAN_MD_DP_LINK_DESC_BANK: Wbm link_desc_bank
 * @WLAN_MD_DP_SRNG_RXDMA_MON_STATUS: dp_srng type for rxdma mon status
 * @WLAN_MD_DP_SRNG_RXDMA_MON_BUF: dp_srng type for rxdma mon buf
 * @WLAN_MD_DP_SRNG_RXDMA_MON_DST: dp_srng type for rxdma mon dest
 * @WLAN_MD_DP_SRNG_RXDMA_MON_DESC: dp_srng type for rxdma mon desc
 * @WLAN_MD_DP_SRNG_RXDMA_ERR_DST: dp_srng type for rxdma err dst
 * @WLAN_MD_DP_HAL_SOC: hal_soc
 * @WLAN_MD_OBJMGR_PSOC: wlan_objmgr_psoc
 * @WLAN_MD_OBJMGR_PSOC_TGT_INFO: wlan_objmgr_tgt_psoc_info
 * @WLAN_MD_OBJMGR_PDEV: wlan_objmgr_pdev
 * @WLAN_MD_OBJMGR_PDEV_MLME: pdev_mlme
 * @WLAN_MD_OBJMGR_VDEV: wlan_objmgr_vdev
 * @WLAN_MD_OBJMGR_VDEV_MLME: vdev mlme
 * @WLAN_MD_OBJMGR_VDEV_SM: wlan_sm
 * @WLAN_MD_DP_SRNG_REO2PPE: dp_srng type PPE rx ring
 * @WLAN_MD_DP_SRNG_PPE2TCL: dp_srng type for PPE tx ring
 * @WLAN_MD_DP_SRNG_PPE_RELEASE: dp_srng type for PPE tx com ring
 * @WLAN_MD_DP_SRNG_PPE_WBM2SW_RELEASE: dp_srng type for PPE2TCL tx com ring
 * @WLAN_MD_DP_SRNG_SW2RXDMA_LINK_RING: dp_srng type for SW2RXDMA link ring
 * @WLAN_MD_MAX: Max value
 */
enum wlan_minidump_host_data {
	WLAN_MD_CP_EXT_PDEV,
	WLAN_MD_CP_EXT_PSOC,
	WLAN_MD_CP_EXT_VDEV,
	WLAN_MD_CP_EXT_PEER,
	WLAN_MD_DP_SOC,
	WLAN_MD_DP_PDEV,
	WLAN_MD_DP_VDEV,
	WLAN_MD_DP_PEER,
	WLAN_MD_DP_SRNG_REO_DEST,
	WLAN_MD_DP_SRNG_REO_EXCEPTION,
	WLAN_MD_DP_SRNG_REO_CMD,
	WLAN_MD_DP_SRNG_RX_REL,
	WLAN_MD_DP_SRNG_REO_REINJECT,
	WLAN_MD_DP_SRNG_REO_STATUS,
	WLAN_MD_DP_SRNG_TCL_DATA,
	WLAN_MD_DP_SRNG_TCL_CMD,
	WLAN_MD_DP_SRNG_TCL_STATUS,
	WLAN_MD_DP_SRNG_TX_COMP,
	WLAN_MD_DP_SRNG_WBM_DESC_REL,
	WLAN_MD_DP_SRNG_WBM_IDLE_LINK,
	WLAN_MD_DP_LINK_DESC_BANK,
	WLAN_MD_DP_SRNG_RXDMA_MON_STATUS,
	WLAN_MD_DP_SRNG_RXDMA_MON_BUF,
	WLAN_MD_DP_SRNG_RXDMA_MON_DST,
	WLAN_MD_DP_SRNG_RXDMA_MON_DESC,
	WLAN_MD_DP_SRNG_RXDMA_ERR_DST,
	WLAN_MD_DP_HAL_SOC,
	WLAN_MD_OBJMGR_PSOC,
	WLAN_MD_OBJMGR_PSOC_TGT_INFO,
	WLAN_MD_OBJMGR_PDEV,
	WLAN_MD_OBJMGR_PDEV_MLME,
	WLAN_MD_OBJMGR_VDEV,
	WLAN_MD_OBJMGR_VDEV_MLME,
	WLAN_MD_OBJMGR_VDEV_SM,
	WLAN_MD_DP_SRNG_REO2PPE,
	WLAN_MD_DP_SRNG_PPE2TCL,
	WLAN_MD_DP_SRNG_PPE_RELEASE,
	WLAN_MD_DP_SRNG_PPE_WBM2SW_RELEASE,
	WLAN_MD_DP_SRNG_SW2RXDMA_LINK_RING,
	WLAN_MD_MAX
};

/**
 * wlan_minidump_log() - Log memory address to be included in minidump
 * @start_addr: Start address of the memory to be dumped
 * @size: Size in bytes
 * @psoc_obj: Psoc Object
 * @type: Type of data structure
 * @name: String to identify this entry
 */
void wlan_minidump_log(void *start_addr, const size_t size,
		       void *psoc_obj,
		       enum wlan_minidump_host_data type,
		       const char *name);

/**
 * wlan_minidump_remove() - Remove memory address from  minidump
 * @start_addr: Start address of the memory previously added
 * @size: Size in bytes
 * @psoc_obj: Psoc Object
 * @type: Type of data structure
 * @name: String to identify this entry
 */
void wlan_minidump_remove(void *start_addr, const size_t size,
			  void *psoc_obj,
			  enum wlan_minidump_host_data type,
			  const char *name);

/**
 * wlan_util_is_vdev_in_cac_wait() - Check if dfs sap vdev is in cac wait
 * @pdev: pdev object
 * @dbg_id: object manager ref id
 *
 * This function checks if dfs sap vdev is in cac wait state
 *
 * Return: true, if cac is in progress, otherwise false
 */
bool wlan_util_is_vdev_in_cac_wait(struct wlan_objmgr_pdev *pdev,
				   wlan_objmgr_ref_dbgid dbg_id);

/**
 * wlan_eht_chan_phy_mode - convert eht chan to phy mode
 * @freq: frequency
 * @bw_val: bandwidth
 * @chan_width: channel width
 *
 * Return: return phy mode
 */
enum wlan_phymode
wlan_eht_chan_phy_mode(uint32_t freq,
		       uint16_t bw_val,
		       enum phy_ch_width chan_width);
#endif /* _WLAN_UTILITY_H_ */

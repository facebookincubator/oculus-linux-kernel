
/* Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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

 
 
 
 
 
 
 


#ifndef _MACTX_HE_SIG_B1_MU_H_
#define _MACTX_HE_SIG_B1_MU_H_
#if !defined(__ASSEMBLER__)
#endif

#include "he_sig_b1_mu_info.h"
#define NUM_OF_DWORDS_MACTX_HE_SIG_B1_MU 2

#define NUM_OF_QWORDS_MACTX_HE_SIG_B1_MU 1


struct mactx_he_sig_b1_mu {
#ifndef WIFI_BIT_ORDER_BIG_ENDIAN
             struct   he_sig_b1_mu_info                                         mactx_he_sig_b1_mu_info_details;
             uint32_t tlv64_padding                                           : 32; // [31:0]
#else
             struct   he_sig_b1_mu_info                                         mactx_he_sig_b1_mu_info_details;
             uint32_t tlv64_padding                                           : 32; // [31:0]
#endif
};


/* Description		MACTX_HE_SIG_B1_MU_INFO_DETAILS

			See detailed description of the STRUCT
*/


/* Description		RU_ALLOCATION

			RU allocation for the user(s) following this common portion
			 of the SIG
			
			For details, refer to  RU_TYPE description
			<legal all>
*/

#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RU_ALLOCATION_OFFSET     0x0000000000000000
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RU_ALLOCATION_LSB        0
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RU_ALLOCATION_MSB        7
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RU_ALLOCATION_MASK       0x00000000000000ff


/* Description		RESERVED_0

			<legal 0>
*/

#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RESERVED_0_OFFSET        0x0000000000000000
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RESERVED_0_LSB           8
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RESERVED_0_MSB           30
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RESERVED_0_MASK          0x000000007fffff00


/* Description		RX_INTEGRITY_CHECK_PASSED

			TX side: Set to 0
			RX side: Set to 1 if PHY determines the CRC check of the
			 codeblock containing the HE-SIG-B common info has passed, 
			else set to 0
			
			<legal all>
*/

#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RX_INTEGRITY_CHECK_PASSED_OFFSET 0x0000000000000000
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RX_INTEGRITY_CHECK_PASSED_LSB 31
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RX_INTEGRITY_CHECK_PASSED_MSB 31
#define MACTX_HE_SIG_B1_MU_MACTX_HE_SIG_B1_MU_INFO_DETAILS_RX_INTEGRITY_CHECK_PASSED_MASK 0x0000000080000000


/* Description		TLV64_PADDING

			Automatic DWORD padding inserted while converting TLV32 
			to TLV64 for 64 bit ARCH
			<legal 0>
*/

#define MACTX_HE_SIG_B1_MU_TLV64_PADDING_OFFSET                                     0x0000000000000000
#define MACTX_HE_SIG_B1_MU_TLV64_PADDING_LSB                                        32
#define MACTX_HE_SIG_B1_MU_TLV64_PADDING_MSB                                        63
#define MACTX_HE_SIG_B1_MU_TLV64_PADDING_MASK                                       0xffffffff00000000



#endif   // MACTX_HE_SIG_B1_MU

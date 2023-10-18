/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _HE_SIG_B1_MU_INFO_H_
#define _HE_SIG_B1_MU_INFO_H_
#if !defined(__ASSEMBLER__)
#endif

#define NUM_OF_DWORDS_HE_SIG_B1_MU_INFO 1


struct he_sig_b1_mu_info {
#ifndef WIFI_BIT_ORDER_BIG_ENDIAN
             uint32_t ru_allocation                                           :  8, // [7:0]
                      reserved_0                                              : 23, // [30:8]
                      rx_integrity_check_passed                               :  1; // [31:31]
#else
             uint32_t rx_integrity_check_passed                               :  1, // [31:31]
                      reserved_0                                              : 23, // [30:8]
                      ru_allocation                                           :  8; // [7:0]
#endif
};


/* Description		RU_ALLOCATION

			RU allocation for the user(s) following this common portion
			 of the SIG
			
			For details, refer to  RU_TYPE description
			<legal all>
*/

#define HE_SIG_B1_MU_INFO_RU_ALLOCATION_OFFSET                                      0x00000000
#define HE_SIG_B1_MU_INFO_RU_ALLOCATION_LSB                                         0
#define HE_SIG_B1_MU_INFO_RU_ALLOCATION_MSB                                         7
#define HE_SIG_B1_MU_INFO_RU_ALLOCATION_MASK                                        0x000000ff


/* Description		RESERVED_0

			<legal 0>
*/

#define HE_SIG_B1_MU_INFO_RESERVED_0_OFFSET                                         0x00000000
#define HE_SIG_B1_MU_INFO_RESERVED_0_LSB                                            8
#define HE_SIG_B1_MU_INFO_RESERVED_0_MSB                                            30
#define HE_SIG_B1_MU_INFO_RESERVED_0_MASK                                           0x7fffff00


/* Description		RX_INTEGRITY_CHECK_PASSED

			TX side: Set to 0
			RX side: Set to 1 if PHY determines the CRC check of the
			 codeblock containing the HE-SIG-B common info has passed, 
			else set to 0
			
			<legal all>
*/

#define HE_SIG_B1_MU_INFO_RX_INTEGRITY_CHECK_PASSED_OFFSET                          0x00000000
#define HE_SIG_B1_MU_INFO_RX_INTEGRITY_CHECK_PASSED_LSB                             31
#define HE_SIG_B1_MU_INFO_RX_INTEGRITY_CHECK_PASSED_MSB                             31
#define HE_SIG_B1_MU_INFO_RX_INTEGRITY_CHECK_PASSED_MASK                            0x80000000



#endif   // HE_SIG_B1_MU_INFO

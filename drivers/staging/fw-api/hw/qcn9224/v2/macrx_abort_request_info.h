
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

 
 
 
 
 
 
 


#ifndef _MACRX_ABORT_REQUEST_INFO_H_
#define _MACRX_ABORT_REQUEST_INFO_H_
#if !defined(__ASSEMBLER__)
#endif

#define NUM_OF_WORDS_MACRX_ABORT_REQUEST_INFO 1


struct macrx_abort_request_info {
#ifndef WIFI_BIT_ORDER_BIG_ENDIAN
             uint16_t macrx_abort_reason                                      :  8,  
                      reserved_0                                              :  8;  
#else
             uint16_t reserved_0                                              :  8,  
                      macrx_abort_reason                                      :  8;  
#endif
};


 

#define MACRX_ABORT_REQUEST_INFO_MACRX_ABORT_REASON_OFFSET                          0x00000000
#define MACRX_ABORT_REQUEST_INFO_MACRX_ABORT_REASON_LSB                             0
#define MACRX_ABORT_REQUEST_INFO_MACRX_ABORT_REASON_MSB                             7
#define MACRX_ABORT_REQUEST_INFO_MACRX_ABORT_REASON_MASK                            0x000000ff


 

#define MACRX_ABORT_REQUEST_INFO_RESERVED_0_OFFSET                                  0x00000000
#define MACRX_ABORT_REQUEST_INFO_RESERVED_0_LSB                                     8
#define MACRX_ABORT_REQUEST_INFO_RESERVED_0_MSB                                     15
#define MACRX_ABORT_REQUEST_INFO_RESERVED_0_MASK                                    0x0000ff00



#endif    

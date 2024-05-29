
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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



#ifndef _BUFFER_ADDR_INFO_H_
#define _BUFFER_ADDR_INFO_H_
#if !defined(__ASSEMBLER__)
#endif

#define NUM_OF_DWORDS_BUFFER_ADDR_INFO 2

struct buffer_addr_info {
             uint32_t buffer_addr_31_0                : 32;
             uint32_t buffer_addr_39_32               :  8,
                      return_buffer_manager           :  3,
                      sw_buffer_cookie                : 21;
};

#define BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_OFFSET                   0x00000000
#define BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_LSB                      0
#define BUFFER_ADDR_INFO_0_BUFFER_ADDR_31_0_MASK                     0xffffffff

#define BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_OFFSET                  0x00000004
#define BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_LSB                     0
#define BUFFER_ADDR_INFO_1_BUFFER_ADDR_39_32_MASK                    0x000000ff

#define BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_OFFSET              0x00000004
#define BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_LSB                 8
#define BUFFER_ADDR_INFO_1_RETURN_BUFFER_MANAGER_MASK                0x00000700

#define BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_OFFSET                   0x00000004
#define BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_LSB                      11
#define BUFFER_ADDR_INFO_1_SW_BUFFER_COOKIE_MASK                     0xfffff800

#endif


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



#ifndef _TCL_STATUS_RING_H_
#define _TCL_STATUS_RING_H_
#if !defined(__ASSEMBLER__)
#endif

#define NUM_OF_DWORDS_TCL_STATUS_RING 8

struct tcl_status_ring {
             uint32_t gse_ctrl                        :  4,
                      ase_fse_sel                     :  1,
                      cache_op_res                    :  2,
                      index_search_en                 :  1,
                      msdu_cnt_n                      : 24;
             uint32_t msdu_byte_cnt_n                 : 32;
             uint32_t msdu_timestmp_n                 : 32;
             uint32_t cmd_meta_data_31_0              : 32;
             uint32_t cmd_meta_data_63_32             : 32;
             uint32_t hash_indx_val                   : 20,
                      cache_set_num                   :  4,
                      reserved_5a                     :  8;
             uint32_t reserved_6a                     : 32;
             uint32_t reserved_7a                     : 20,
                      ring_id                         :  8,
                      looping_count                   :  4;
};

#define TCL_STATUS_RING_0_GSE_CTRL_OFFSET                            0x00000000
#define TCL_STATUS_RING_0_GSE_CTRL_LSB                               0
#define TCL_STATUS_RING_0_GSE_CTRL_MASK                              0x0000000f

#define TCL_STATUS_RING_0_ASE_FSE_SEL_OFFSET                         0x00000000
#define TCL_STATUS_RING_0_ASE_FSE_SEL_LSB                            4
#define TCL_STATUS_RING_0_ASE_FSE_SEL_MASK                           0x00000010

#define TCL_STATUS_RING_0_CACHE_OP_RES_OFFSET                        0x00000000
#define TCL_STATUS_RING_0_CACHE_OP_RES_LSB                           5
#define TCL_STATUS_RING_0_CACHE_OP_RES_MASK                          0x00000060

#define TCL_STATUS_RING_0_INDEX_SEARCH_EN_OFFSET                     0x00000000
#define TCL_STATUS_RING_0_INDEX_SEARCH_EN_LSB                        7
#define TCL_STATUS_RING_0_INDEX_SEARCH_EN_MASK                       0x00000080

#define TCL_STATUS_RING_0_MSDU_CNT_N_OFFSET                          0x00000000
#define TCL_STATUS_RING_0_MSDU_CNT_N_LSB                             8
#define TCL_STATUS_RING_0_MSDU_CNT_N_MASK                            0xffffff00

#define TCL_STATUS_RING_1_MSDU_BYTE_CNT_N_OFFSET                     0x00000004
#define TCL_STATUS_RING_1_MSDU_BYTE_CNT_N_LSB                        0
#define TCL_STATUS_RING_1_MSDU_BYTE_CNT_N_MASK                       0xffffffff

#define TCL_STATUS_RING_2_MSDU_TIMESTMP_N_OFFSET                     0x00000008
#define TCL_STATUS_RING_2_MSDU_TIMESTMP_N_LSB                        0
#define TCL_STATUS_RING_2_MSDU_TIMESTMP_N_MASK                       0xffffffff

#define TCL_STATUS_RING_3_CMD_META_DATA_31_0_OFFSET                  0x0000000c
#define TCL_STATUS_RING_3_CMD_META_DATA_31_0_LSB                     0
#define TCL_STATUS_RING_3_CMD_META_DATA_31_0_MASK                    0xffffffff

#define TCL_STATUS_RING_4_CMD_META_DATA_63_32_OFFSET                 0x00000010
#define TCL_STATUS_RING_4_CMD_META_DATA_63_32_LSB                    0
#define TCL_STATUS_RING_4_CMD_META_DATA_63_32_MASK                   0xffffffff

#define TCL_STATUS_RING_5_HASH_INDX_VAL_OFFSET                       0x00000014
#define TCL_STATUS_RING_5_HASH_INDX_VAL_LSB                          0
#define TCL_STATUS_RING_5_HASH_INDX_VAL_MASK                         0x000fffff

#define TCL_STATUS_RING_5_CACHE_SET_NUM_OFFSET                       0x00000014
#define TCL_STATUS_RING_5_CACHE_SET_NUM_LSB                          20
#define TCL_STATUS_RING_5_CACHE_SET_NUM_MASK                         0x00f00000

#define TCL_STATUS_RING_5_RESERVED_5A_OFFSET                         0x00000014
#define TCL_STATUS_RING_5_RESERVED_5A_LSB                            24
#define TCL_STATUS_RING_5_RESERVED_5A_MASK                           0xff000000

#define TCL_STATUS_RING_6_RESERVED_6A_OFFSET                         0x00000018
#define TCL_STATUS_RING_6_RESERVED_6A_LSB                            0
#define TCL_STATUS_RING_6_RESERVED_6A_MASK                           0xffffffff

#define TCL_STATUS_RING_7_RESERVED_7A_OFFSET                         0x0000001c
#define TCL_STATUS_RING_7_RESERVED_7A_LSB                            0
#define TCL_STATUS_RING_7_RESERVED_7A_MASK                           0x000fffff

#define TCL_STATUS_RING_7_RING_ID_OFFSET                             0x0000001c
#define TCL_STATUS_RING_7_RING_ID_LSB                                20
#define TCL_STATUS_RING_7_RING_ID_MASK                               0x0ff00000

#define TCL_STATUS_RING_7_LOOPING_COUNT_OFFSET                       0x0000001c
#define TCL_STATUS_RING_7_LOOPING_COUNT_LSB                          28
#define TCL_STATUS_RING_7_LOOPING_COUNT_MASK                         0xf0000000

#endif

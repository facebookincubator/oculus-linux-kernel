
/* Copyright (c) 2021, Qualcomm Innovation Center, Inc. All rights reserved.
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

 
 
 
 
 
 
 


#ifndef _RX_LOCATION_INFO_H_
#define _RX_LOCATION_INFO_H_
#if !defined(__ASSEMBLER__)
#endif

#define NUM_OF_DWORDS_RX_LOCATION_INFO 28


struct rx_location_info {
#ifndef WIFI_BIT_ORDER_BIG_ENDIAN
             uint32_t rx_location_info_valid                                  :  1,  
                      rtt_hw_ifft_mode                                        :  1,  
                      rtt_11az_mode                                           :  2,  
                      reserved_0                                              :  4,  
                      rtt_num_fac                                             :  8,  
                      rtt_rx_chain_mask                                       :  8,  
                      rtt_num_streams                                         :  8;  
             uint32_t rtt_first_selected_chain                                :  8,  
                      rtt_second_selected_chain                               :  8,  
                      rtt_cfr_status                                          :  8,  
                      rtt_cir_status                                          :  8;  
             uint32_t rtt_che_buffer_pointer_low32                            : 32;  
             uint32_t rtt_che_buffer_pointer_high8                            :  8,  
                      reserved_3                                              :  8,  
                      rtt_pkt_bw_vht                                          :  4,  
                      rtt_pkt_bw_leg                                          :  4,  
                      rtt_mcs_rate                                            :  8;  
             uint32_t rtt_cfo_measurement                                     : 16,  
                      rtt_preamble_type                                       :  8,  
                      rtt_gi_type                                             :  8;  
             uint32_t rx_start_ts                                             : 32;  
             uint32_t rx_start_ts_upper                                       : 32;  
             uint32_t rx_end_ts                                               : 32;  
             uint32_t gain_chain0                                             : 16,  
                      gain_chain1                                             : 16;  
             uint32_t gain_chain2                                             : 16,  
                      gain_chain3                                             : 16;  
             uint32_t gain_report_status                                      :  8,  
                      rtt_timing_backoff_sel                                  :  8,  
                      rtt_fac_combined                                        : 16;  
             uint32_t rtt_fac_0                                               : 16,  
                      rtt_fac_1                                               : 16;  
             uint32_t rtt_fac_2                                               : 16,  
                      rtt_fac_3                                               : 16;  
             uint32_t rtt_fac_4                                               : 16,  
                      rtt_fac_5                                               : 16;  
             uint32_t rtt_fac_6                                               : 16,  
                      rtt_fac_7                                               : 16;  
             uint32_t rtt_fac_8                                               : 16,  
                      rtt_fac_9                                               : 16;  
             uint32_t rtt_fac_10                                              : 16,  
                      rtt_fac_11                                              : 16;  
             uint32_t rtt_fac_12                                              : 16,  
                      rtt_fac_13                                              : 16;  
             uint32_t rtt_fac_14                                              : 16,  
                      rtt_fac_15                                              : 16;  
             uint32_t rtt_fac_16                                              : 16,  
                      rtt_fac_17                                              : 16;  
             uint32_t rtt_fac_18                                              : 16,  
                      rtt_fac_19                                              : 16;  
             uint32_t rtt_fac_20                                              : 16,  
                      rtt_fac_21                                              : 16;  
             uint32_t rtt_fac_22                                              : 16,  
                      rtt_fac_23                                              : 16;  
             uint32_t rtt_fac_24                                              : 16,  
                      rtt_fac_25                                              : 16;  
             uint32_t rtt_fac_26                                              : 16,  
                      rtt_fac_27                                              : 16;  
             uint32_t rtt_fac_28                                              : 16,  
                      rtt_fac_29                                              : 16;  
             uint32_t rtt_fac_30                                              : 16,  
                      rtt_fac_31                                              : 16;  
             uint32_t reserved_27a                                            : 32;  
#else
             uint32_t rtt_num_streams                                         :  8,  
                      rtt_rx_chain_mask                                       :  8,  
                      rtt_num_fac                                             :  8,  
                      reserved_0                                              :  4,  
                      rtt_11az_mode                                           :  2,  
                      rtt_hw_ifft_mode                                        :  1,  
                      rx_location_info_valid                                  :  1;  
             uint32_t rtt_cir_status                                          :  8,  
                      rtt_cfr_status                                          :  8,  
                      rtt_second_selected_chain                               :  8,  
                      rtt_first_selected_chain                                :  8;  
             uint32_t rtt_che_buffer_pointer_low32                            : 32;  
             uint32_t rtt_mcs_rate                                            :  8,  
                      rtt_pkt_bw_leg                                          :  4,  
                      rtt_pkt_bw_vht                                          :  4,  
                      reserved_3                                              :  8,  
                      rtt_che_buffer_pointer_high8                            :  8;  
             uint32_t rtt_gi_type                                             :  8,  
                      rtt_preamble_type                                       :  8,  
                      rtt_cfo_measurement                                     : 16;  
             uint32_t rx_start_ts                                             : 32;  
             uint32_t rx_start_ts_upper                                       : 32;  
             uint32_t rx_end_ts                                               : 32;  
             uint32_t gain_chain1                                             : 16,  
                      gain_chain0                                             : 16;  
             uint32_t gain_chain3                                             : 16,  
                      gain_chain2                                             : 16;  
             uint32_t rtt_fac_combined                                        : 16,  
                      rtt_timing_backoff_sel                                  :  8,  
                      gain_report_status                                      :  8;  
             uint32_t rtt_fac_1                                               : 16,  
                      rtt_fac_0                                               : 16;  
             uint32_t rtt_fac_3                                               : 16,  
                      rtt_fac_2                                               : 16;  
             uint32_t rtt_fac_5                                               : 16,  
                      rtt_fac_4                                               : 16;  
             uint32_t rtt_fac_7                                               : 16,  
                      rtt_fac_6                                               : 16;  
             uint32_t rtt_fac_9                                               : 16,  
                      rtt_fac_8                                               : 16;  
             uint32_t rtt_fac_11                                              : 16,  
                      rtt_fac_10                                              : 16;  
             uint32_t rtt_fac_13                                              : 16,  
                      rtt_fac_12                                              : 16;  
             uint32_t rtt_fac_15                                              : 16,  
                      rtt_fac_14                                              : 16;  
             uint32_t rtt_fac_17                                              : 16,  
                      rtt_fac_16                                              : 16;  
             uint32_t rtt_fac_19                                              : 16,  
                      rtt_fac_18                                              : 16;  
             uint32_t rtt_fac_21                                              : 16,  
                      rtt_fac_20                                              : 16;  
             uint32_t rtt_fac_23                                              : 16,  
                      rtt_fac_22                                              : 16;  
             uint32_t rtt_fac_25                                              : 16,  
                      rtt_fac_24                                              : 16;  
             uint32_t rtt_fac_27                                              : 16,  
                      rtt_fac_26                                              : 16;  
             uint32_t rtt_fac_29                                              : 16,  
                      rtt_fac_28                                              : 16;  
             uint32_t rtt_fac_31                                              : 16,  
                      rtt_fac_30                                              : 16;  
             uint32_t reserved_27a                                            : 32;  
#endif
};


 

#define RX_LOCATION_INFO_RX_LOCATION_INFO_VALID_OFFSET                              0x00000000
#define RX_LOCATION_INFO_RX_LOCATION_INFO_VALID_LSB                                 0
#define RX_LOCATION_INFO_RX_LOCATION_INFO_VALID_MSB                                 0
#define RX_LOCATION_INFO_RX_LOCATION_INFO_VALID_MASK                                0x00000001


 

#define RX_LOCATION_INFO_RTT_HW_IFFT_MODE_OFFSET                                    0x00000000
#define RX_LOCATION_INFO_RTT_HW_IFFT_MODE_LSB                                       1
#define RX_LOCATION_INFO_RTT_HW_IFFT_MODE_MSB                                       1
#define RX_LOCATION_INFO_RTT_HW_IFFT_MODE_MASK                                      0x00000002


 

#define RX_LOCATION_INFO_RTT_11AZ_MODE_OFFSET                                       0x00000000
#define RX_LOCATION_INFO_RTT_11AZ_MODE_LSB                                          2
#define RX_LOCATION_INFO_RTT_11AZ_MODE_MSB                                          3
#define RX_LOCATION_INFO_RTT_11AZ_MODE_MASK                                         0x0000000c


 

#define RX_LOCATION_INFO_RESERVED_0_OFFSET                                          0x00000000
#define RX_LOCATION_INFO_RESERVED_0_LSB                                             4
#define RX_LOCATION_INFO_RESERVED_0_MSB                                             7
#define RX_LOCATION_INFO_RESERVED_0_MASK                                            0x000000f0


 

#define RX_LOCATION_INFO_RTT_NUM_FAC_OFFSET                                         0x00000000
#define RX_LOCATION_INFO_RTT_NUM_FAC_LSB                                            8
#define RX_LOCATION_INFO_RTT_NUM_FAC_MSB                                            15
#define RX_LOCATION_INFO_RTT_NUM_FAC_MASK                                           0x0000ff00


 

#define RX_LOCATION_INFO_RTT_RX_CHAIN_MASK_OFFSET                                   0x00000000
#define RX_LOCATION_INFO_RTT_RX_CHAIN_MASK_LSB                                      16
#define RX_LOCATION_INFO_RTT_RX_CHAIN_MASK_MSB                                      23
#define RX_LOCATION_INFO_RTT_RX_CHAIN_MASK_MASK                                     0x00ff0000


 

#define RX_LOCATION_INFO_RTT_NUM_STREAMS_OFFSET                                     0x00000000
#define RX_LOCATION_INFO_RTT_NUM_STREAMS_LSB                                        24
#define RX_LOCATION_INFO_RTT_NUM_STREAMS_MSB                                        31
#define RX_LOCATION_INFO_RTT_NUM_STREAMS_MASK                                       0xff000000


 

#define RX_LOCATION_INFO_RTT_FIRST_SELECTED_CHAIN_OFFSET                            0x00000004
#define RX_LOCATION_INFO_RTT_FIRST_SELECTED_CHAIN_LSB                               0
#define RX_LOCATION_INFO_RTT_FIRST_SELECTED_CHAIN_MSB                               7
#define RX_LOCATION_INFO_RTT_FIRST_SELECTED_CHAIN_MASK                              0x000000ff


 

#define RX_LOCATION_INFO_RTT_SECOND_SELECTED_CHAIN_OFFSET                           0x00000004
#define RX_LOCATION_INFO_RTT_SECOND_SELECTED_CHAIN_LSB                              8
#define RX_LOCATION_INFO_RTT_SECOND_SELECTED_CHAIN_MSB                              15
#define RX_LOCATION_INFO_RTT_SECOND_SELECTED_CHAIN_MASK                             0x0000ff00


 

#define RX_LOCATION_INFO_RTT_CFR_STATUS_OFFSET                                      0x00000004
#define RX_LOCATION_INFO_RTT_CFR_STATUS_LSB                                         16
#define RX_LOCATION_INFO_RTT_CFR_STATUS_MSB                                         23
#define RX_LOCATION_INFO_RTT_CFR_STATUS_MASK                                        0x00ff0000


 

#define RX_LOCATION_INFO_RTT_CIR_STATUS_OFFSET                                      0x00000004
#define RX_LOCATION_INFO_RTT_CIR_STATUS_LSB                                         24
#define RX_LOCATION_INFO_RTT_CIR_STATUS_MSB                                         31
#define RX_LOCATION_INFO_RTT_CIR_STATUS_MASK                                        0xff000000


 

#define RX_LOCATION_INFO_RTT_CHE_BUFFER_POINTER_LOW32_OFFSET                        0x00000008
#define RX_LOCATION_INFO_RTT_CHE_BUFFER_POINTER_LOW32_LSB                           0
#define RX_LOCATION_INFO_RTT_CHE_BUFFER_POINTER_LOW32_MSB                           31
#define RX_LOCATION_INFO_RTT_CHE_BUFFER_POINTER_LOW32_MASK                          0xffffffff


 

#define RX_LOCATION_INFO_RTT_CHE_BUFFER_POINTER_HIGH8_OFFSET                        0x0000000c
#define RX_LOCATION_INFO_RTT_CHE_BUFFER_POINTER_HIGH8_LSB                           0
#define RX_LOCATION_INFO_RTT_CHE_BUFFER_POINTER_HIGH8_MSB                           7
#define RX_LOCATION_INFO_RTT_CHE_BUFFER_POINTER_HIGH8_MASK                          0x000000ff


 

#define RX_LOCATION_INFO_RESERVED_3_OFFSET                                          0x0000000c
#define RX_LOCATION_INFO_RESERVED_3_LSB                                             8
#define RX_LOCATION_INFO_RESERVED_3_MSB                                             15
#define RX_LOCATION_INFO_RESERVED_3_MASK                                            0x0000ff00


 

#define RX_LOCATION_INFO_RTT_PKT_BW_VHT_OFFSET                                      0x0000000c
#define RX_LOCATION_INFO_RTT_PKT_BW_VHT_LSB                                         16
#define RX_LOCATION_INFO_RTT_PKT_BW_VHT_MSB                                         19
#define RX_LOCATION_INFO_RTT_PKT_BW_VHT_MASK                                        0x000f0000


 

#define RX_LOCATION_INFO_RTT_PKT_BW_LEG_OFFSET                                      0x0000000c
#define RX_LOCATION_INFO_RTT_PKT_BW_LEG_LSB                                         20
#define RX_LOCATION_INFO_RTT_PKT_BW_LEG_MSB                                         23
#define RX_LOCATION_INFO_RTT_PKT_BW_LEG_MASK                                        0x00f00000


 

#define RX_LOCATION_INFO_RTT_MCS_RATE_OFFSET                                        0x0000000c
#define RX_LOCATION_INFO_RTT_MCS_RATE_LSB                                           24
#define RX_LOCATION_INFO_RTT_MCS_RATE_MSB                                           31
#define RX_LOCATION_INFO_RTT_MCS_RATE_MASK                                          0xff000000


 

#define RX_LOCATION_INFO_RTT_CFO_MEASUREMENT_OFFSET                                 0x00000010
#define RX_LOCATION_INFO_RTT_CFO_MEASUREMENT_LSB                                    0
#define RX_LOCATION_INFO_RTT_CFO_MEASUREMENT_MSB                                    15
#define RX_LOCATION_INFO_RTT_CFO_MEASUREMENT_MASK                                   0x0000ffff


 

#define RX_LOCATION_INFO_RTT_PREAMBLE_TYPE_OFFSET                                   0x00000010
#define RX_LOCATION_INFO_RTT_PREAMBLE_TYPE_LSB                                      16
#define RX_LOCATION_INFO_RTT_PREAMBLE_TYPE_MSB                                      23
#define RX_LOCATION_INFO_RTT_PREAMBLE_TYPE_MASK                                     0x00ff0000


 

#define RX_LOCATION_INFO_RTT_GI_TYPE_OFFSET                                         0x00000010
#define RX_LOCATION_INFO_RTT_GI_TYPE_LSB                                            24
#define RX_LOCATION_INFO_RTT_GI_TYPE_MSB                                            31
#define RX_LOCATION_INFO_RTT_GI_TYPE_MASK                                           0xff000000


 

#define RX_LOCATION_INFO_RX_START_TS_OFFSET                                         0x00000014
#define RX_LOCATION_INFO_RX_START_TS_LSB                                            0
#define RX_LOCATION_INFO_RX_START_TS_MSB                                            31
#define RX_LOCATION_INFO_RX_START_TS_MASK                                           0xffffffff


 

#define RX_LOCATION_INFO_RX_START_TS_UPPER_OFFSET                                   0x00000018
#define RX_LOCATION_INFO_RX_START_TS_UPPER_LSB                                      0
#define RX_LOCATION_INFO_RX_START_TS_UPPER_MSB                                      31
#define RX_LOCATION_INFO_RX_START_TS_UPPER_MASK                                     0xffffffff


 

#define RX_LOCATION_INFO_RX_END_TS_OFFSET                                           0x0000001c
#define RX_LOCATION_INFO_RX_END_TS_LSB                                              0
#define RX_LOCATION_INFO_RX_END_TS_MSB                                              31
#define RX_LOCATION_INFO_RX_END_TS_MASK                                             0xffffffff


 

#define RX_LOCATION_INFO_GAIN_CHAIN0_OFFSET                                         0x00000020
#define RX_LOCATION_INFO_GAIN_CHAIN0_LSB                                            0
#define RX_LOCATION_INFO_GAIN_CHAIN0_MSB                                            15
#define RX_LOCATION_INFO_GAIN_CHAIN0_MASK                                           0x0000ffff


 

#define RX_LOCATION_INFO_GAIN_CHAIN1_OFFSET                                         0x00000020
#define RX_LOCATION_INFO_GAIN_CHAIN1_LSB                                            16
#define RX_LOCATION_INFO_GAIN_CHAIN1_MSB                                            31
#define RX_LOCATION_INFO_GAIN_CHAIN1_MASK                                           0xffff0000


 

#define RX_LOCATION_INFO_GAIN_CHAIN2_OFFSET                                         0x00000024
#define RX_LOCATION_INFO_GAIN_CHAIN2_LSB                                            0
#define RX_LOCATION_INFO_GAIN_CHAIN2_MSB                                            15
#define RX_LOCATION_INFO_GAIN_CHAIN2_MASK                                           0x0000ffff


 

#define RX_LOCATION_INFO_GAIN_CHAIN3_OFFSET                                         0x00000024
#define RX_LOCATION_INFO_GAIN_CHAIN3_LSB                                            16
#define RX_LOCATION_INFO_GAIN_CHAIN3_MSB                                            31
#define RX_LOCATION_INFO_GAIN_CHAIN3_MASK                                           0xffff0000


 

#define RX_LOCATION_INFO_GAIN_REPORT_STATUS_OFFSET                                  0x00000028
#define RX_LOCATION_INFO_GAIN_REPORT_STATUS_LSB                                     0
#define RX_LOCATION_INFO_GAIN_REPORT_STATUS_MSB                                     7
#define RX_LOCATION_INFO_GAIN_REPORT_STATUS_MASK                                    0x000000ff


 

#define RX_LOCATION_INFO_RTT_TIMING_BACKOFF_SEL_OFFSET                              0x00000028
#define RX_LOCATION_INFO_RTT_TIMING_BACKOFF_SEL_LSB                                 8
#define RX_LOCATION_INFO_RTT_TIMING_BACKOFF_SEL_MSB                                 15
#define RX_LOCATION_INFO_RTT_TIMING_BACKOFF_SEL_MASK                                0x0000ff00


 

#define RX_LOCATION_INFO_RTT_FAC_COMBINED_OFFSET                                    0x00000028
#define RX_LOCATION_INFO_RTT_FAC_COMBINED_LSB                                       16
#define RX_LOCATION_INFO_RTT_FAC_COMBINED_MSB                                       31
#define RX_LOCATION_INFO_RTT_FAC_COMBINED_MASK                                      0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_0_OFFSET                                           0x0000002c
#define RX_LOCATION_INFO_RTT_FAC_0_LSB                                              0
#define RX_LOCATION_INFO_RTT_FAC_0_MSB                                              15
#define RX_LOCATION_INFO_RTT_FAC_0_MASK                                             0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_1_OFFSET                                           0x0000002c
#define RX_LOCATION_INFO_RTT_FAC_1_LSB                                              16
#define RX_LOCATION_INFO_RTT_FAC_1_MSB                                              31
#define RX_LOCATION_INFO_RTT_FAC_1_MASK                                             0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_2_OFFSET                                           0x00000030
#define RX_LOCATION_INFO_RTT_FAC_2_LSB                                              0
#define RX_LOCATION_INFO_RTT_FAC_2_MSB                                              15
#define RX_LOCATION_INFO_RTT_FAC_2_MASK                                             0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_3_OFFSET                                           0x00000030
#define RX_LOCATION_INFO_RTT_FAC_3_LSB                                              16
#define RX_LOCATION_INFO_RTT_FAC_3_MSB                                              31
#define RX_LOCATION_INFO_RTT_FAC_3_MASK                                             0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_4_OFFSET                                           0x00000034
#define RX_LOCATION_INFO_RTT_FAC_4_LSB                                              0
#define RX_LOCATION_INFO_RTT_FAC_4_MSB                                              15
#define RX_LOCATION_INFO_RTT_FAC_4_MASK                                             0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_5_OFFSET                                           0x00000034
#define RX_LOCATION_INFO_RTT_FAC_5_LSB                                              16
#define RX_LOCATION_INFO_RTT_FAC_5_MSB                                              31
#define RX_LOCATION_INFO_RTT_FAC_5_MASK                                             0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_6_OFFSET                                           0x00000038
#define RX_LOCATION_INFO_RTT_FAC_6_LSB                                              0
#define RX_LOCATION_INFO_RTT_FAC_6_MSB                                              15
#define RX_LOCATION_INFO_RTT_FAC_6_MASK                                             0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_7_OFFSET                                           0x00000038
#define RX_LOCATION_INFO_RTT_FAC_7_LSB                                              16
#define RX_LOCATION_INFO_RTT_FAC_7_MSB                                              31
#define RX_LOCATION_INFO_RTT_FAC_7_MASK                                             0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_8_OFFSET                                           0x0000003c
#define RX_LOCATION_INFO_RTT_FAC_8_LSB                                              0
#define RX_LOCATION_INFO_RTT_FAC_8_MSB                                              15
#define RX_LOCATION_INFO_RTT_FAC_8_MASK                                             0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_9_OFFSET                                           0x0000003c
#define RX_LOCATION_INFO_RTT_FAC_9_LSB                                              16
#define RX_LOCATION_INFO_RTT_FAC_9_MSB                                              31
#define RX_LOCATION_INFO_RTT_FAC_9_MASK                                             0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_10_OFFSET                                          0x00000040
#define RX_LOCATION_INFO_RTT_FAC_10_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_10_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_10_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_11_OFFSET                                          0x00000040
#define RX_LOCATION_INFO_RTT_FAC_11_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_11_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_11_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_12_OFFSET                                          0x00000044
#define RX_LOCATION_INFO_RTT_FAC_12_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_12_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_12_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_13_OFFSET                                          0x00000044
#define RX_LOCATION_INFO_RTT_FAC_13_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_13_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_13_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_14_OFFSET                                          0x00000048
#define RX_LOCATION_INFO_RTT_FAC_14_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_14_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_14_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_15_OFFSET                                          0x00000048
#define RX_LOCATION_INFO_RTT_FAC_15_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_15_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_15_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_16_OFFSET                                          0x0000004c
#define RX_LOCATION_INFO_RTT_FAC_16_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_16_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_16_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_17_OFFSET                                          0x0000004c
#define RX_LOCATION_INFO_RTT_FAC_17_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_17_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_17_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_18_OFFSET                                          0x00000050
#define RX_LOCATION_INFO_RTT_FAC_18_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_18_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_18_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_19_OFFSET                                          0x00000050
#define RX_LOCATION_INFO_RTT_FAC_19_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_19_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_19_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_20_OFFSET                                          0x00000054
#define RX_LOCATION_INFO_RTT_FAC_20_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_20_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_20_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_21_OFFSET                                          0x00000054
#define RX_LOCATION_INFO_RTT_FAC_21_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_21_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_21_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_22_OFFSET                                          0x00000058
#define RX_LOCATION_INFO_RTT_FAC_22_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_22_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_22_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_23_OFFSET                                          0x00000058
#define RX_LOCATION_INFO_RTT_FAC_23_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_23_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_23_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_24_OFFSET                                          0x0000005c
#define RX_LOCATION_INFO_RTT_FAC_24_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_24_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_24_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_25_OFFSET                                          0x0000005c
#define RX_LOCATION_INFO_RTT_FAC_25_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_25_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_25_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_26_OFFSET                                          0x00000060
#define RX_LOCATION_INFO_RTT_FAC_26_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_26_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_26_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_27_OFFSET                                          0x00000060
#define RX_LOCATION_INFO_RTT_FAC_27_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_27_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_27_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_28_OFFSET                                          0x00000064
#define RX_LOCATION_INFO_RTT_FAC_28_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_28_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_28_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_29_OFFSET                                          0x00000064
#define RX_LOCATION_INFO_RTT_FAC_29_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_29_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_29_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RTT_FAC_30_OFFSET                                          0x00000068
#define RX_LOCATION_INFO_RTT_FAC_30_LSB                                             0
#define RX_LOCATION_INFO_RTT_FAC_30_MSB                                             15
#define RX_LOCATION_INFO_RTT_FAC_30_MASK                                            0x0000ffff


 

#define RX_LOCATION_INFO_RTT_FAC_31_OFFSET                                          0x00000068
#define RX_LOCATION_INFO_RTT_FAC_31_LSB                                             16
#define RX_LOCATION_INFO_RTT_FAC_31_MSB                                             31
#define RX_LOCATION_INFO_RTT_FAC_31_MASK                                            0xffff0000


 

#define RX_LOCATION_INFO_RESERVED_27A_OFFSET                                        0x0000006c
#define RX_LOCATION_INFO_RESERVED_27A_LSB                                           0
#define RX_LOCATION_INFO_RESERVED_27A_MSB                                           31
#define RX_LOCATION_INFO_RESERVED_27A_MASK                                          0xffffffff



#endif    

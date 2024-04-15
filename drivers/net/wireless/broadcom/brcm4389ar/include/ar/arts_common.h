/*
 * (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
 */

/***************************************************************************************************
 *
 * @file arts_common.h
 *
 * @brief Common ARTS packet related logic
 *
 * @details
 *
 **************************************************************************************************/

#ifndef _meta_arts_common_h_
#define _meta_arts_common_h_

/***************************************************************************************************
 * Macro Definitions
 **************************************************************************************************/

/* Check magic number in the packet body */
#define ARTS_ETH_HEAD_MAGIC 0xf2ac

#define AR_IS_ARTS_PACKET(eh) ((eh)->ether_type == hton16(ETH_P_802_EX1))

#endif /*_meta_arts_common_h_*/

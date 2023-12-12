/*
 * Copyright Meta Platforms, Inc. and its affiliates.
 *
 * NOTICE OF CONFIDENTIAL AND PROPRIETARY INFORMATION & TECHNOLOGY:
 * The information and technology contained herein (including the accompanying binary code)
 * is the confidential information of Meta Platforms, Inc. and its affiliates (collectively,
 * "Meta"). It is protected by applicable copyright and trade secret law, and may be claimed
 * in one or more U.S. or foreign patents or pending patent applications. Meta retains all right,
 * title and interest (including all intellectual property rights) in such information and
 * technology, and no licenses are hereby granted by Meta. Unauthorized use, reproduction, or
 * dissemination is a violation of Meta's rights and is strictly prohibited.
 */

/***************************************************************************************************
 *
 * @file arts_priority.h
 *
 * @brief This is Meta specific custom header for deriving skb priority from the ARTS header
 *
 * @details
 *
 **************************************************************************************************/

#ifndef _meta_arts_priority_h_
#define _meta_arts_priority_h_

/***************************************************************************************************
 * Includes
 **************************************************************************************************/
#include <ethernet.h>

#ifdef CONFIG_AR_ARTS_SKB_PRIORITY
#include "ar/include/ARTS_header.h"
#endif // CONFIG_AR_ARTS_SKB_PRIORITY

/***************************************************************************************************
 * Macro Definitions
 **************************************************************************************************/
#ifdef CONFIG_AR_ARTS_SKB_PRIORITY

#define CONFIG_ETH_DESG_HEADROOM 16
/* Check magic number in the packet body */
#define ARTS_ETH_HEAD_MAGIC 0xf2ac

#define AR_IS_ARTS_PACKET(eh) ((eh)->ether_type == hton16(ETH_P_802_EX1))

#define AR_EVAL_ARTS_PRIORITY(pktdata, priority)                           \
    uint8 *arts_pktbody = (pktdata) + sizeof(struct ether_header);         \
    if (*((uint16 *)(arts_pktbody)) == hton16(ARTS_ETH_HEAD_MAGIC)) {      \
        ARTS_header_v1_t *arts_header =                                    \
            (ARTS_header_v1_t *)(arts_pktbody + CONFIG_ETH_DESG_HEADROOM); \
        (priority) = (arts_header->qos << 1);                              \
    }

#else // CONFIG_AR_ARTS_SKB_PRIORITY

#define AR_IS_ARTS_PACKET(...) (false)
#define AR_EVAL_ARTS_PRIORITY(...)

#endif // CONFIG_AR_ARTS_SKB_PRIORITY

/***************************************************************************************************
 * Type Definitions
 **************************************************************************************************/

/***************************************************************************************************
 * Constant Definitions
 **************************************************************************************************/

/***************************************************************************************************
 * Exported Variable Declarations
 **************************************************************************************************/

/***************************************************************************************************
 * Exported Function Prototypes
 **************************************************************************************************/

#endif /*_meta_arts_priority_h_*/

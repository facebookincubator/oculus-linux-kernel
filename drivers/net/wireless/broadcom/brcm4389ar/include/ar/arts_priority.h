/*
 * (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
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
#include "ar/arts_common.h"
#endif /* CONFIG_AR_ARTS_SKB_PRIORITY */

/***************************************************************************************************
 * Macro Definitions
 **************************************************************************************************/
#ifdef CONFIG_AR_ARTS_SKB_PRIORITY

#define AR_EVAL_ARTS_PRIORITY(pktdata, priority)                          \
    do {                                                                  \
        uint8 *arts_pktbody = (pktdata) + sizeof(struct ether_header);    \
        if (*((uint16 *)(arts_pktbody)) == hton16(ARTS_ETH_HEAD_MAGIC)) { \
            ARTS_header_t *arts_header = (ARTS_header_t *)(arts_pktbody); \
            (priority)                 = (arts_header->qos << 1);         \
        }                                                                 \
    } while (0)

#else /* CONFIG_AR_ARTS_SKB_PRIORITY */

#define AR_IS_ARTS_PACKET(...) (false)
#define AR_EVAL_ARTS_PRIORITY(...)

#endif /* CONFIG_AR_ARTS_SKB_PRIORITY */

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

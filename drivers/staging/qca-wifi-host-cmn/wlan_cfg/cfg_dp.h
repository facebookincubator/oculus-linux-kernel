/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: This file contains definitions of Data Path configuration.
 */

#ifndef _CFG_DP_H_
#define _CFG_DP_H_

#include "cfg_define.h"
#include "wlan_init_cfg.h"

#define WLAN_CFG_MAX_CLIENTS 64
#define WLAN_CFG_MAX_CLIENTS_MIN 8
#define WLAN_CFG_MAX_CLIENTS_MAX 64

/* Change this to a lower value to enforce scattered idle list mode */
#define WLAN_CFG_MAX_ALLOC_SIZE 0x200000
#define WLAN_CFG_MAX_ALLOC_SIZE_MIN 0x80000
#define WLAN_CFG_MAX_ALLOC_SIZE_MAX 0x200000

#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || \
	defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
#define WLAN_CFG_TX_FLOW_START_QUEUE_OFFSET 10
#define WLAN_CFG_TX_FLOW_STOP_QUEUE_TH 15
#else
#define WLAN_CFG_TX_FLOW_START_QUEUE_OFFSET 0
#define WLAN_CFG_TX_FLOW_STOP_QUEUE_TH 0
#endif

#define WLAN_CFG_PER_PDEV_TX_RING_MIN 0
#define WLAN_CFG_PER_PDEV_TX_RING_MAX 1

#ifdef IPA_OFFLOAD
/* Size of TCL TX Ring */
#if defined(TX_TO_NPEERS_INC_TX_DESCS)
#define WLAN_CFG_TX_RING_SIZE 2048
#else
#define WLAN_CFG_TX_RING_SIZE 1024
#endif

#define WLAN_CFG_IPA_TX_RING_SIZE_MIN 512
#define WLAN_CFG_IPA_TX_RING_SIZE 1024
#define WLAN_CFG_IPA_TX_RING_SIZE_MAX 0x80000

#define WLAN_CFG_IPA_TX_COMP_RING_SIZE_MIN 512
#define WLAN_CFG_IPA_TX_COMP_RING_SIZE 1024
#define WLAN_CFG_IPA_TX_COMP_RING_SIZE_MAX 0x80000

#ifdef IPA_WDI3_TX_TWO_PIPES
#ifdef WLAN_MEMORY_OPT
#define WLAN_CFG_IPA_TX_ALT_RING_SIZE_MIN 128
#else
#define WLAN_CFG_IPA_TX_ALT_RING_SIZE_MIN 512
#endif
#define WLAN_CFG_IPA_TX_ALT_RING_SIZE 1024
#define WLAN_CFG_IPA_TX_ALT_RING_SIZE_MAX 0x80000

#ifdef WLAN_MEMORY_OPT
#define WLAN_CFG_IPA_TX_ALT_COMP_RING_SIZE_MIN 128
#else
#define WLAN_CFG_IPA_TX_ALT_COMP_RING_SIZE_MIN 512
#endif
#define WLAN_CFG_IPA_TX_ALT_COMP_RING_SIZE 1024
#define WLAN_CFG_IPA_TX_ALT_COMP_RING_SIZE_MAX 0x80000
#endif

#define WLAN_CFG_PER_PDEV_TX_RING 0
#define WLAN_CFG_IPA_UC_TX_BUF_SIZE 2048
#define WLAN_CFG_IPA_UC_TX_PARTITION_BASE 3000
#define WLAN_CFG_IPA_UC_RX_IND_RING_COUNT 1024
#else
#define WLAN_CFG_TX_RING_SIZE 512
#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
#define WLAN_CFG_PER_PDEV_TX_RING 1
#else
#define WLAN_CFG_PER_PDEV_TX_RING 0
#endif
#define WLAN_CFG_IPA_UC_TX_BUF_SIZE 0
#define WLAN_CFG_IPA_UC_TX_PARTITION_BASE 0
#define WLAN_CFG_IPA_UC_RX_IND_RING_COUNT 0
#endif /* IPA_OFFLOAD */

#define WLAN_CFG_TIME_CONTROL_BP 3000

#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
#define WLAN_CFG_PER_PDEV_RX_RING 0
#define WLAN_CFG_PER_PDEV_LMAC_RING 0
#define WLAN_LRO_ENABLE 0
#ifdef QCA_WIFI_QCA6750
#define WLAN_CFG_MAC_PER_TARGET 1
#else
#define WLAN_CFG_MAC_PER_TARGET 2
#endif

#if defined(TX_TO_NPEERS_INC_TX_DESCS)
#define WLAN_CFG_TX_COMP_RING_SIZE 4096

/* Tx Descriptor and Tx Extension Descriptor pool sizes */
#define WLAN_CFG_NUM_TX_DESC  4096
#define WLAN_CFG_NUM_TX_EXT_DESC 4096
#else
#define WLAN_CFG_TX_COMP_RING_SIZE 1024

/* Tx Descriptor and Tx Extension Descriptor pool sizes */
#define WLAN_CFG_NUM_TX_DESC  1024
#define WLAN_CFG_NUM_TX_EXT_DESC 1024
#endif

/* Interrupt Mitigation - Batch threshold in terms of number of frames */
#define WLAN_CFG_INT_BATCH_THRESHOLD_TX 1
#define WLAN_CFG_INT_BATCH_THRESHOLD_OTHER 1

/* Interrupt Mitigation - Timer threshold in us */
#define WLAN_CFG_INT_TIMER_THRESHOLD_TX 8
#define WLAN_CFG_INT_TIMER_THRESHOLD_OTHER 8

#ifdef WLAN_DP_PER_RING_TYPE_CONFIG
#define WLAN_CFG_INT_BATCH_THRESHOLD_RX \
		WLAN_CFG_INT_BATCH_THRESHOLD_REO_RING
#define WLAN_CFG_INT_TIMER_THRESHOLD_RX \
		WLAN_CFG_INT_TIMER_THRESHOLD_REO_RING
#else
#define WLAN_CFG_INT_BATCH_THRESHOLD_RX 1
#define WLAN_CFG_INT_TIMER_THRESHOLD_RX 8
#endif
#endif /* WLAN_MAX_PDEVS */

#define WLAN_CFG_INT_BATCH_THRESHOLD_PPE2TCL 0
#define WLAN_CFG_INT_TIMER_THRESHOLD_PPE2TCL 30

#ifdef NBUF_MEMORY_DEBUG
#define WLAN_CFG_RX_PENDING_THRESHOLD_DEFAULT 0xFFFF
#else
#define WLAN_CFG_RX_PENDING_THRESHOLD_DEFAULT 0x1FFFF
#endif

#define WLAN_CFG_RX_PENDING_HL_THRESHOLD \
		WLAN_CFG_RX_PENDING_THRESHOLD_DEFAULT
#define WLAN_CFG_RX_PENDING_HL_THRESHOLD_MIN 0
#define WLAN_CFG_RX_PENDING_HL_THRESHOLD_MAX 0x200000

#define WLAN_CFG_RX_PENDING_LO_THRESHOLD \
		WLAN_CFG_RX_PENDING_THRESHOLD_DEFAULT
#define WLAN_CFG_RX_PENDING_LO_THRESHOLD_MIN 100
#define WLAN_CFG_RX_PENDING_LO_THRESHOLD_MAX 0x200000

#define WLAN_CFG_INT_TIMER_THRESHOLD_WBM_RELEASE_RING 256
#define WLAN_CFG_INT_TIMER_THRESHOLD_REO_RING 512
#define WLAN_CFG_INT_BATCH_THRESHOLD_REO_RING 0

#define WLAN_CFG_PER_PDEV_RX_RING_MIN 0
#define WLAN_CFG_PER_PDEV_RX_RING_MAX 0

#define WLAN_CFG_PER_PDEV_LMAC_RING_MIN 0
#define WLAN_CFG_PER_PDEV_LMAC_RING_MAX 1

#define WLAN_CFG_TX_RING_SIZE_MIN 512
#define WLAN_CFG_TX_RING_SIZE_MAX 0x80000

#define WLAN_CFG_TIME_CONTROL_BP_MIN 3000
#define WLAN_CFG_TIME_CONTROL_BP_MAX 1800000

#define WLAN_CFG_TX_COMP_RING_SIZE_MIN 512
#define WLAN_CFG_TX_COMP_RING_SIZE_MAX 0x80000

#define WLAN_CFG_NUM_TX_DESC_MIN  16
#define WLAN_CFG_NUM_TX_DESC_MAX  0x10000

#define WLAN_CFG_NUM_TX_SPL_DESC  1024
#define WLAN_CFG_NUM_TX_SPL_DESC_MIN  0
#define WLAN_CFG_NUM_TX_SPL_DESC_MAX  0x1000

#define WLAN_CFG_NUM_TX_EXT_DESC_MIN  16
#define WLAN_CFG_NUM_TX_EXT_DESC_MAX  0x80000

#define WLAN_CFG_INT_BATCH_THRESHOLD_TX_MIN 1
#define WLAN_CFG_INT_BATCH_THRESHOLD_TX_MAX 256

#define WLAN_CFG_INT_BATCH_THRESHOLD_PPE2TCL_MIN 0
#define WLAN_CFG_INT_BATCH_THRESHOLD_PPE2TCL_MAX 1024

#define WLAN_CFG_INT_BATCH_THRESHOLD_RX_MIN 0
#define WLAN_CFG_INT_BATCH_THRESHOLD_RX_MAX 128

#define WLAN_CFG_INT_BATCH_THRESHOLD_REO_RING_MIN 1
#define WLAN_CFG_INT_BATCH_THRESHOLD_REO_RING_MAX 128

#define WLAN_CFG_INT_BATCH_THRESHOLD_WBM_RELEASE_RING_MIN 1
#define WLAN_CFG_INT_BATCH_THRESHOLD_WBM_RELEASE_RING_MAX 128

#define WLAN_CFG_INT_BATCH_THRESHOLD_OTHER_MIN 1
#define WLAN_CFG_INT_BATCH_THRESHOLD_OTHER_MAX 1

#define WLAN_CFG_INT_TIMER_THRESHOLD_TX_MIN 8
#define WLAN_CFG_INT_TIMER_THRESHOLD_TX_MAX 1000

#define WLAN_CFG_INT_TIMER_THRESHOLD_PPE2TCL_MIN 8
#define WLAN_CFG_INT_TIMER_THRESHOLD_PPE2TCL_MAX 1000

#define WLAN_CFG_INT_TIMER_THRESHOLD_RX_MIN 8
#define WLAN_CFG_INT_TIMER_THRESHOLD_RX_MAX 500

#define WLAN_CFG_INT_TIMER_THRESHOLD_OTHER_MIN 8
#define WLAN_CFG_INT_TIMER_THRESHOLD_OTHER_MAX 1000

#define WLAN_CFG_INT_TIMER_THRESHOLD_REO_RING_MIN 8
#define WLAN_CFG_INT_TIMER_THRESHOLD_REO_RING_MAX 512

#define WLAN_CFG_INT_TIMER_THRESHOLD_WBM_RELEASE_RING_MIN 8
#define WLAN_CFG_INT_TIMER_THRESHOLD_WBM_RELEASE_RING_MAX 500

#define WLAN_CFG_NSS_TX_COMP_RING_SIZE 0x2000
#define WLAN_CFG_NSS_TX_COMP_RING_SIZE_MIN 0x2000
#define WLAN_CFG_NSS_TX_COMP_RING_SIZE_MAX 0xc000

#ifdef QCA_LL_TX_FLOW_CONTROL_V2

/* Per vdev pools */
#define WLAN_CFG_NUM_TX_DESC_POOL	3
#define WLAN_CFG_NUM_TXEXT_DESC_POOL	3

#else /* QCA_LL_TX_FLOW_CONTROL_V2 */

#ifdef TX_PER_PDEV_DESC_POOL
#define WLAN_CFG_NUM_TX_DESC_POOL	MAX_PDEV_CNT
#define WLAN_CFG_NUM_TXEXT_DESC_POOL	MAX_PDEV_CNT

#else /* TX_PER_PDEV_DESC_POOL */

#define WLAN_CFG_NUM_TX_DESC_POOL 3
#define WLAN_CFG_NUM_TXEXT_DESC_POOL 3

#endif /* TX_PER_PDEV_DESC_POOL */
#endif /* QCA_LL_TX_FLOW_CONTROL_V2 */

#define WLAN_CFG_NUM_TXEXT_DESC_POOL_MIN 1
#define WLAN_CFG_NUM_TXEXT_DESC_POOL_MAX 4

#define WLAN_CFG_HTT_PKT_TYPE 2
#define WLAN_CFG_HTT_PKT_TYPE_MIN 2
#define WLAN_CFG_HTT_PKT_TYPE_MAX 2

#define WLAN_CFG_MAX_PEER_ID 64
#define WLAN_CFG_MAX_PEER_ID_MIN 64
#define WLAN_CFG_MAX_PEER_ID_MAX 64

#define WLAN_CFG_RX_DEFRAG_TIMEOUT 100
#define WLAN_CFG_RX_DEFRAG_TIMEOUT_MIN 100
#define WLAN_CFG_RX_DEFRAG_TIMEOUT_MAX 100

#define WLAN_CFG_NUM_TCL_DATA_RINGS 3
#define WLAN_CFG_NUM_TCL_DATA_RINGS_MIN 1
#define WLAN_CFG_NUM_TCL_DATA_RINGS_MAX MAX_TCL_DATA_RINGS

#define WLAN_CFG_NUM_TX_COMP_RINGS WLAN_CFG_NUM_TCL_DATA_RINGS
#define WLAN_CFG_NUM_TX_COMP_RINGS_MIN WLAN_CFG_NUM_TCL_DATA_RINGS_MIN
#define WLAN_CFG_NUM_TX_COMP_RINGS_MAX WLAN_CFG_NUM_TCL_DATA_RINGS_MAX

#if defined(CONFIG_BERYLLIUM)
#define WLAN_CFG_NUM_REO_DEST_RING 8
#else
#define WLAN_CFG_NUM_REO_DEST_RING 4
#endif
#define WLAN_CFG_NUM_REO_DEST_RING_MIN 4
#define WLAN_CFG_NUM_REO_DEST_RING_MAX MAX_REO_DEST_RINGS

#define WLAN_CFG_NSS_NUM_TCL_DATA_RINGS 2
#define WLAN_CFG_NSS_NUM_TCL_DATA_RINGS_MIN 1
#define WLAN_CFG_NSS_NUM_TCL_DATA_RINGS_MAX 3

#define WLAN_CFG_NSS_NUM_REO_DEST_RING 2
#define WLAN_CFG_NSS_NUM_REO_DEST_RING_MIN 1
#define WLAN_CFG_NSS_NUM_REO_DEST_RING_MAX 3

#define WLAN_CFG_WBM_RELEASE_RING_SIZE 1024
#define WLAN_CFG_WBM_RELEASE_RING_SIZE_MIN 64
#define WLAN_CFG_WBM_RELEASE_RING_SIZE_MAX 1024

#define WLAN_CFG_TCL_CMD_CREDIT_RING_SIZE 512
#define WLAN_CFG_TCL_CMD_CREDIT_RING_SIZE_MIN 32
#define WLAN_CFG_TCL_CMD_CREDIT_RING_SIZE_MAX 512

#define WLAN_CFG_TCL_STATUS_RING_SIZE 32
#define WLAN_CFG_TCL_STATUS_RING_SIZE_MIN 32
#define WLAN_CFG_TCL_STATUS_RING_SIZE_MAX 32

#if defined(QCA_WIFI_QCA6290)
#define WLAN_CFG_REO_DST_RING_SIZE 1024
#else
#define WLAN_CFG_REO_DST_RING_SIZE 2048
#endif

#define WLAN_CFG_REO_DST_RING_SIZE_MIN 8
#define WLAN_CFG_REO_DST_RING_SIZE_MAX 8192

#define WLAN_CFG_REO_REINJECT_RING_SIZE 128
#define WLAN_CFG_REO_REINJECT_RING_SIZE_MIN 32
#define WLAN_CFG_REO_REINJECT_RING_SIZE_MAX 128

#define WLAN_CFG_RX_RELEASE_RING_SIZE 1024
#define WLAN_CFG_RX_RELEASE_RING_SIZE_MIN 8
#if defined(QCA_WIFI_QCA6390) || defined(QCA_WIFI_QCA6490) || \
    defined(QCA_WIFI_QCA6750) || defined(QCA_WIFI_KIWI)
#define WLAN_CFG_RX_RELEASE_RING_SIZE_MAX 1024
#else
#define WLAN_CFG_RX_RELEASE_RING_SIZE_MAX 32768
#endif

#define WLAN_CFG_REO_EXCEPTION_RING_SIZE 256
#define WLAN_CFG_REO_EXCEPTION_RING_SIZE_MIN 128
#define WLAN_CFG_REO_EXCEPTION_RING_SIZE_MAX 512

#define WLAN_CFG_REO_CMD_RING_SIZE 128
#define WLAN_CFG_REO_CMD_RING_SIZE_MIN 64
#define WLAN_CFG_REO_CMD_RING_SIZE_MAX 128

#define WLAN_CFG_REO_STATUS_RING_SIZE 256
#define WLAN_CFG_REO_STATUS_RING_SIZE_MIN 128
#define WLAN_CFG_REO_STATUS_RING_SIZE_MAX 2048

#define WLAN_CFG_RXDMA_BUF_RING_SIZE 1024
#ifdef WLAN_MEMORY_OPT
#define WLAN_CFG_RXDMA_BUF_RING_SIZE_MIN 128
#else
#define WLAN_CFG_RXDMA_BUF_RING_SIZE_MIN 1024
#endif
#define WLAN_CFG_RXDMA_BUF_RING_SIZE_MAX 4096

#define WLAN_CFG_RXDMA_REFILL_RING_SIZE 4096
#define WLAN_CFG_RXDMA_REFILL_RING_SIZE_MIN 16
#define WLAN_CFG_RXDMA_REFILL_RING_SIZE_MAX 16384

#define WLAN_CFG_TX_DESC_LIMIT_0 0
#define WLAN_CFG_TX_DESC_LIMIT_0_MIN 4096
#define WLAN_CFG_TX_DESC_LIMIT_0_MAX 32768

#define WLAN_CFG_TX_DESC_LIMIT_1 0
#define WLAN_CFG_TX_DESC_LIMIT_1_MIN 4096
#define WLAN_CFG_TX_DESC_LIMIT_1_MAX 32768

#define WLAN_CFG_TX_DESC_LIMIT_2 0
#define WLAN_CFG_TX_DESC_LIMIT_2_MIN 4096
#define WLAN_CFG_TX_DESC_LIMIT_2_MAX 32768

#define WLAN_CFG_TX_DEVICE_LIMIT 65536
#define WLAN_CFG_TX_DEVICE_LIMIT_MIN 16384
#define WLAN_CFG_TX_DEVICE_LIMIT_MAX 65536

#define WLAN_CFG_TX_SPL_DEVICE_LIMIT 1024
#define WLAN_CFG_TX_SPL_DEVICE_LIMIT_MIN 0
#define WLAN_CFG_TX_SPL_DEVICE_LIMIT_MAX 4096

#define WLAN_CFG_TX_SW_INTERNODE_QUEUE 1024
#define WLAN_CFG_TX_SW_INTERNODE_QUEUE_MIN 128
#define WLAN_CFG_TX_SW_INTERNODE_QUEUE_MAX 1024

#define WLAN_CFG_RXDMA_MONITOR_BUF_RING_SIZE 4096
#define WLAN_CFG_RXDMA_MONITOR_BUF_RING_SIZE_MIN 16
#define WLAN_CFG_RXDMA_MONITOR_BUF_RING_SIZE_MAX 8192

#define WLAN_CFG_TX_MONITOR_BUF_RING_SIZE 4096
#define WLAN_CFG_TX_MONITOR_BUF_RING_SIZE_MIN 16
#define WLAN_CFG_TX_MONITOR_BUF_RING_SIZE_MAX 8192

#define WLAN_CFG_RXDMA_MONITOR_DST_RING_SIZE 2048
#define WLAN_CFG_RXDMA_MONITOR_DST_RING_SIZE_MIN 48
#define WLAN_CFG_RXDMA_MONITOR_DST_RING_SIZE_MAX 8192

#define WLAN_CFG_TX_MONITOR_DST_RING_SIZE 2048
#define WLAN_CFG_TX_MONITOR_DST_RING_SIZE_MIN 48
#define WLAN_CFG_TX_MONITOR_DST_RING_SIZE_MAX 8192

#define WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE 1024
#define WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE_MIN 16
#define WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE_MAX 8192

#define WLAN_CFG_RXDMA_MONITOR_DESC_RING_SIZE 4096
#define WLAN_CFG_RXDMA_MONITOR_DESC_RING_SIZE_MIN 4096
#define WLAN_CFG_RXDMA_MONITOR_DESC_RING_SIZE_MAX 16384

#define WLAN_CFG_RXDMA_ERR_DST_RING_SIZE 1024
#define WLAN_CFG_RXDMA_ERR_DST_RING_SIZE_MIN 1024
#define WLAN_CFG_RXDMA_ERR_DST_RING_SIZE_MAX 8192

#define WLAN_CFG_RXDMA_MONITOR_RX_DROP_THRESH_SIZE 32
#define WLAN_CFG_RXDMA_MONITOR_RX_DROP_THRESH_SIZE_MIN 0
#define WLAN_CFG_RXDMA_MONITOR_RX_DROP_THRESH_SIZE_MAX 256

/*
 * Allocate as many RX descriptors as buffers in the SW2RXDMA
 * ring. This value may need to be tuned later.
 */
#if defined(WLAN_MAX_PDEVS) && (WLAN_MAX_PDEVS == 1)
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE 1
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE_MIN 1
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE_MAX 1
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE 4096
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE_MIN 1024
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE_MAX 16384

/*
 * For low memory AP cases using 1 will reduce the rx descriptors memory req
 */
#elif defined(QCA_LOWMEM_CONFIG) || defined(QCA_512M_CONFIG)
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE 1
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE_MIN 1
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE_MAX 3
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE 4096
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE_MIN 1024
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE_MAX 16384

/*
 * AP use cases need to allocate more RX Descriptors than the number of
 * entries available in the SW2RXDMA buffer replenish ring. This is to account
 * for frames sitting in REO queues, HW-HW DMA rings etc. Hence using a
 * multiplication factor of 3, to allocate three times as many RX descriptors
 * as RX buffers.
 */
#else
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE 3
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE_MIN 1
#define WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE_MAX 3
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE 12288
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE_MIN 4096
#define WLAN_CFG_RX_SW_DESC_NUM_SIZE_MAX 16384
#endif

#define WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE 16384
#define WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE_MIN 1
#define WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE_MAX 16384
#define WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE_DEFAULT 128

#define WLAN_CFG_PKTLOG_BUFFER_SIZE 10
#define WLAN_CFG_PKTLOG_MIN_BUFFER_SIZE 1
#define WLAN_CFG_PKTLOG_MAX_BUFFER_SIZE 10

#ifdef IPA_OFFLOAD
#define WLAN_CFG_NUM_REO_RINGS_MAP 0x7
#else
#define WLAN_CFG_NUM_REO_RINGS_MAP 0xF
#endif
#define WLAN_CFG_NUM_REO_RINGS_MAP_MIN 0x1
#if defined(CONFIG_BERYLLIUM)
#define WLAN_CFG_NUM_REO_RINGS_MAP_MAX 0xFF
#else
#define WLAN_CFG_NUM_REO_RINGS_MAP_MAX 0xF
#endif

#define WLAN_CFG_RADIO_0_DEFAULT_REO 0x1
#define WLAN_CFG_RADIO_1_DEFAULT_REO 0x2
#define WLAN_CFG_RADIO_2_DEFAULT_REO 0x3

#define WLAN_CFG_RADIO_DEFAULT_REO_MIN 0x1
#define WLAN_CFG_RADIO_DEFAULT_REO_MAX 0x4

#define WLAN_CFG_REO2PPE_RING_SIZE 8192
#define WLAN_CFG_REO2PPE_RING_SIZE_MIN 64
#define WLAN_CFG_REO2PPE_RING_SIZE_MAX 16384

#define WLAN_CFG_PPE2TCL_RING_SIZE 8192
#define WLAN_CFG_PPE2TCL_RING_SIZE_MIN 64
#define WLAN_CFG_PPE2TCL_RING_SIZE_MAX 32768

#define WLAN_CFG_PPE_RELEASE_RING_SIZE 1024
#define WLAN_CFG_PPE_RELEASE_RING_SIZE_MIN 64
#define WLAN_CFG_PPE_RELEASE_RING_SIZE_MAX 1024

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
#define WLAN_CFG_MLO_RX_RING_MAP 0x7
#define WLAN_CFG_MLO_RX_RING_MAP_MIN 0x0
#define WLAN_CFG_MLO_RX_RING_MAP_MAX 0xFF
#endif

#define WLAN_CFG_TX_CAPT_MAX_MEM_MIN 0
#define WLAN_CFG_TX_CAPT_MAX_MEM_MAX 512
#define WLAN_CFG_TX_CAPT_MAX_MEM_DEFAULT 0

#define CFG_DP_MPDU_RETRY_THRESHOLD_MIN 0
#define CFG_DP_MPDU_RETRY_THRESHOLD_MAX 255
#define CFG_DP_MPDU_RETRY_THRESHOLD 0

#define WLAN_CFG_DP_NAPI_SCALE_FACTOR 0
#define WLAN_CFG_DP_NAPI_SCALE_FACTOR_MIN 0
#define WLAN_CFG_DP_NAPI_SCALE_FACTOR_MAX 4

#ifdef CONFIG_SAWF_STATS
#define WLAN_CFG_SAWF_STATS 0x0
#define WLAN_CFG_SAWF_STATS_MIN 0x0
#define WLAN_CFG_SAWF_STATS_MAX 0x7
#endif
/*
 * <ini>
 * "dp_tx_capt_max_mem_mb"- maximum memory used by Tx capture
 * @Min: 0
 * @Max: 512 MB
 * @Default: 0 (disabled)
 *
 * This ini entry is used to set a max limit beyond which frames
 * are dropped by Tx capture. User needs to set a non-zero value
 * to enable it.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_TX_CAPT_MAX_MEM_MB \
		CFG_INI_UINT("dp_tx_capt_max_mem_mb", \
		WLAN_CFG_TX_CAPT_MAX_MEM_MIN, \
		WLAN_CFG_TX_CAPT_MAX_MEM_MAX, \
		WLAN_CFG_TX_CAPT_MAX_MEM_DEFAULT, \
			CFG_VALUE_OR_DEFAULT, "Max Memory (in MB) used by Tx Capture")

/* DP INI Declarations */
#define CFG_DP_HTT_PACKET_TYPE \
		CFG_INI_UINT("dp_htt_packet_type", \
		WLAN_CFG_HTT_PKT_TYPE_MIN, \
		WLAN_CFG_HTT_PKT_TYPE_MAX, \
		WLAN_CFG_HTT_PKT_TYPE, \
		CFG_VALUE_OR_DEFAULT, "DP HTT packet type")

#define CFG_DP_INT_BATCH_THRESHOLD_OTHER \
		CFG_INI_UINT("dp_int_batch_threshold_other", \
		WLAN_CFG_INT_BATCH_THRESHOLD_OTHER_MIN, \
		WLAN_CFG_INT_BATCH_THRESHOLD_OTHER_MAX, \
		WLAN_CFG_INT_BATCH_THRESHOLD_OTHER, \
		CFG_VALUE_OR_DEFAULT, "DP INT batch threshold Other")

#define CFG_DP_INT_BATCH_THRESHOLD_RX \
		CFG_INI_UINT("dp_int_batch_threshold_rx", \
		WLAN_CFG_INT_BATCH_THRESHOLD_RX_MIN, \
		WLAN_CFG_INT_BATCH_THRESHOLD_RX_MAX, \
		WLAN_CFG_INT_BATCH_THRESHOLD_RX, \
		CFG_VALUE_OR_DEFAULT, "DP INT batch threshold Rx")

#define CFG_DP_INT_BATCH_THRESHOLD_TX \
		CFG_INI_UINT("dp_int_batch_threshold_tx", \
		WLAN_CFG_INT_BATCH_THRESHOLD_TX_MIN, \
		WLAN_CFG_INT_BATCH_THRESHOLD_TX_MAX, \
		WLAN_CFG_INT_BATCH_THRESHOLD_TX, \
		CFG_VALUE_OR_DEFAULT, "DP INT threshold Tx")

#define CFG_DP_INT_BATCH_THRESHOLD_PPE2TCL \
		CFG_INI_UINT("dp_int_batch_threshold_ppe2tcl", \
		WLAN_CFG_INT_BATCH_THRESHOLD_PPE2TCL_MIN, \
		WLAN_CFG_INT_BATCH_THRESHOLD_PPE2TCL_MAX, \
		WLAN_CFG_INT_BATCH_THRESHOLD_PPE2TCL, \
		CFG_VALUE_OR_DEFAULT, "DP INT batch threshold ppe2tcl")

#define CFG_DP_INT_TIMER_THRESHOLD_PPE2TCL \
		CFG_INI_UINT("dp_int_timer_threshold_ppe2tcl", \
		WLAN_CFG_INT_TIMER_THRESHOLD_PPE2TCL_MIN, \
		WLAN_CFG_INT_TIMER_THRESHOLD_PPE2TCL_MAX, \
		WLAN_CFG_INT_TIMER_THRESHOLD_PPE2TCL, \
		CFG_VALUE_OR_DEFAULT, "DP INT timer threshold ppe2tcl")

#define CFG_DP_INT_TIMER_THRESHOLD_OTHER \
		CFG_INI_UINT("dp_int_timer_threshold_other", \
		WLAN_CFG_INT_TIMER_THRESHOLD_OTHER_MIN, \
		WLAN_CFG_INT_TIMER_THRESHOLD_OTHER_MAX, \
		WLAN_CFG_INT_TIMER_THRESHOLD_OTHER, \
		CFG_VALUE_OR_DEFAULT, "DP INT timer threshold Other")

#define CFG_DP_INT_TIMER_THRESHOLD_RX \
		CFG_INI_UINT("dp_int_timer_threshold_rx", \
		WLAN_CFG_INT_TIMER_THRESHOLD_RX_MIN, \
		WLAN_CFG_INT_TIMER_THRESHOLD_RX_MAX, \
		WLAN_CFG_INT_TIMER_THRESHOLD_RX, \
		CFG_VALUE_OR_DEFAULT, "DP INT timer threshold Rx")

#define CFG_DP_INT_TIMER_THRESHOLD_REO_RING \
		CFG_INI_UINT("dp_int_timer_threshold_reo_ring", \
		WLAN_CFG_INT_TIMER_THRESHOLD_REO_RING_MIN, \
		WLAN_CFG_INT_TIMER_THRESHOLD_REO_RING_MAX, \
		WLAN_CFG_INT_TIMER_THRESHOLD_REO_RING, \
		CFG_VALUE_OR_DEFAULT, "DP INT timer threshold Reo ring")

#define CFG_DP_INT_TIMER_THRESHOLD_WBM_RELEASE_RING \
		CFG_INI_UINT("dp_int_timer_threshold_wbm_release_ring", \
		WLAN_CFG_INT_TIMER_THRESHOLD_WBM_RELEASE_RING_MIN, \
		WLAN_CFG_INT_TIMER_THRESHOLD_WBM_RELEASE_RING_MAX, \
		WLAN_CFG_INT_TIMER_THRESHOLD_WBM_RELEASE_RING, \
		CFG_VALUE_OR_DEFAULT, "DP INT timer threshold wbm release ring")

#define CFG_DP_INT_TIMER_THRESHOLD_TX \
		CFG_INI_UINT("dp_int_timer_threshold_tx", \
		WLAN_CFG_INT_TIMER_THRESHOLD_TX_MIN, \
		WLAN_CFG_INT_TIMER_THRESHOLD_TX_MAX, \
		WLAN_CFG_INT_TIMER_THRESHOLD_TX, \
		CFG_VALUE_OR_DEFAULT, "DP INT timer threshold Tx")

#define CFG_DP_MAX_ALLOC_SIZE \
		CFG_INI_UINT("dp_max_alloc_size", \
		WLAN_CFG_MAX_ALLOC_SIZE_MIN, \
		WLAN_CFG_MAX_ALLOC_SIZE_MAX, \
		WLAN_CFG_MAX_ALLOC_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP Max Alloc Size")

#define CFG_DP_MAX_CLIENTS \
		CFG_INI_UINT("dp_max_clients", \
		WLAN_CFG_MAX_CLIENTS_MIN, \
		WLAN_CFG_MAX_CLIENTS_MAX, \
		WLAN_CFG_MAX_CLIENTS, \
		CFG_VALUE_OR_DEFAULT, "DP Max Clients")

#define CFG_DP_MAX_PEER_ID \
		CFG_INI_UINT("dp_max_peer_id", \
		WLAN_CFG_MAX_PEER_ID_MIN, \
		WLAN_CFG_MAX_PEER_ID_MAX, \
		WLAN_CFG_MAX_PEER_ID, \
		CFG_VALUE_OR_DEFAULT, "DP Max Peer ID")

#define CFG_DP_REO_DEST_RINGS \
		CFG_INI_UINT("dp_reo_dest_rings", \
		WLAN_CFG_NUM_REO_DEST_RING_MIN, \
		WLAN_CFG_NUM_REO_DEST_RING_MAX, \
		WLAN_CFG_NUM_REO_DEST_RING, \
		CFG_VALUE_OR_DEFAULT, "DP REO Destination Rings")

#define CFG_DP_TX_COMP_RINGS \
		CFG_INI_UINT("dp_tx_comp_rings", \
		WLAN_CFG_NUM_TX_COMP_RINGS_MIN, \
		WLAN_CFG_NUM_TX_COMP_RINGS_MAX, \
		WLAN_CFG_NUM_TX_COMP_RINGS, \
		CFG_VALUE_OR_DEFAULT, "DP Tx Comp Rings")

#define CFG_DP_TCL_DATA_RINGS \
		CFG_INI_UINT("dp_tcl_data_rings", \
		WLAN_CFG_NUM_TCL_DATA_RINGS_MIN, \
		WLAN_CFG_NUM_TCL_DATA_RINGS_MAX, \
		WLAN_CFG_NUM_TCL_DATA_RINGS, \
		CFG_VALUE_OR_DEFAULT, "DP TCL Data Rings")

#define CFG_DP_NSS_REO_DEST_RINGS \
		CFG_INI_UINT("dp_nss_reo_dest_rings", \
		WLAN_CFG_NSS_NUM_REO_DEST_RING_MIN, \
		WLAN_CFG_NSS_NUM_REO_DEST_RING_MAX, \
		WLAN_CFG_NSS_NUM_REO_DEST_RING, \
		CFG_VALUE_OR_DEFAULT, "DP NSS REO Destination Rings")

#define CFG_DP_NSS_TCL_DATA_RINGS \
		CFG_INI_UINT("dp_nss_tcl_data_rings", \
		WLAN_CFG_NSS_NUM_TCL_DATA_RINGS_MIN, \
		WLAN_CFG_NSS_NUM_TCL_DATA_RINGS_MAX, \
		WLAN_CFG_NSS_NUM_TCL_DATA_RINGS, \
		CFG_VALUE_OR_DEFAULT, "DP NSS TCL Data Rings")

#define CFG_DP_TX_DESC \
		CFG_INI_UINT("dp_tx_desc", \
		WLAN_CFG_NUM_TX_DESC_MIN, \
		WLAN_CFG_NUM_TX_DESC_MAX, \
		WLAN_CFG_NUM_TX_DESC, \
		CFG_VALUE_OR_DEFAULT, "DP Tx Descriptors")

#define CFG_DP_TX_SPL_DESC \
		CFG_INI_UINT("dp_tx_spl_desc", \
		WLAN_CFG_NUM_TX_SPL_DESC_MIN, \
		WLAN_CFG_NUM_TX_SPL_DESC_MAX, \
		WLAN_CFG_NUM_TX_SPL_DESC, \
		CFG_VALUE_OR_DEFAULT, "DP Tx Special Descriptors")

#define CFG_DP_TX_EXT_DESC \
		CFG_INI_UINT("dp_tx_ext_desc", \
		WLAN_CFG_NUM_TX_EXT_DESC_MIN, \
		WLAN_CFG_NUM_TX_EXT_DESC_MAX, \
		WLAN_CFG_NUM_TX_EXT_DESC, \
		CFG_VALUE_OR_DEFAULT, "DP Tx Ext Descriptors")

#define CFG_DP_TX_EXT_DESC_POOLS \
		CFG_INI_UINT("dp_tx_ext_desc_pool", \
		WLAN_CFG_NUM_TXEXT_DESC_POOL_MIN, \
		WLAN_CFG_NUM_TXEXT_DESC_POOL_MAX, \
		WLAN_CFG_NUM_TXEXT_DESC_POOL, \
		CFG_VALUE_OR_DEFAULT, "DP Tx Ext Descriptors Pool")

#define CFG_DP_PDEV_RX_RING \
		CFG_INI_UINT("dp_pdev_rx_ring", \
		WLAN_CFG_PER_PDEV_RX_RING_MIN, \
		WLAN_CFG_PER_PDEV_RX_RING_MAX, \
		WLAN_CFG_PER_PDEV_RX_RING, \
		CFG_VALUE_OR_DEFAULT, "DP PDEV Rx Ring")

#define CFG_DP_PDEV_TX_RING \
		CFG_INI_UINT("dp_pdev_tx_ring", \
		WLAN_CFG_PER_PDEV_TX_RING_MIN, \
		WLAN_CFG_PER_PDEV_TX_RING_MAX, \
		WLAN_CFG_PER_PDEV_TX_RING, \
		CFG_VALUE_OR_DEFAULT, \
		"DP PDEV Tx Ring")

#define CFG_DP_RX_DEFRAG_TIMEOUT \
		CFG_INI_UINT("dp_rx_defrag_timeout", \
		WLAN_CFG_RX_DEFRAG_TIMEOUT_MIN, \
		WLAN_CFG_RX_DEFRAG_TIMEOUT_MAX, \
		WLAN_CFG_RX_DEFRAG_TIMEOUT, \
		CFG_VALUE_OR_DEFAULT, "DP Rx Defrag Timeout")

#define CFG_DP_TX_COMPL_RING_SIZE \
		CFG_INI_UINT("dp_tx_compl_ring_size", \
		WLAN_CFG_TX_COMP_RING_SIZE_MIN, \
		WLAN_CFG_TX_COMP_RING_SIZE_MAX, \
		WLAN_CFG_TX_COMP_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP Tx Completion Ring Size")

#define CFG_DP_TX_RING_SIZE \
		CFG_INI_UINT("dp_tx_ring_size", \
		WLAN_CFG_TX_RING_SIZE_MIN,\
		WLAN_CFG_TX_RING_SIZE_MAX,\
		WLAN_CFG_TX_RING_SIZE,\
		CFG_VALUE_OR_DEFAULT, "DP Tx Ring Size")

#define CFG_DP_NSS_COMP_RING_SIZE \
		CFG_INI_UINT("dp_nss_comp_ring_size", \
		WLAN_CFG_NSS_TX_COMP_RING_SIZE_MIN, \
		WLAN_CFG_NSS_TX_COMP_RING_SIZE_MAX, \
		WLAN_CFG_NSS_TX_COMP_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP NSS completion Ring Size")

#define CFG_DP_PDEV_LMAC_RING \
		CFG_INI_UINT("dp_pdev_lmac_ring", \
		WLAN_CFG_PER_PDEV_LMAC_RING_MIN, \
		WLAN_CFG_PER_PDEV_LMAC_RING_MAX, \
		WLAN_CFG_PER_PDEV_LMAC_RING, \
		CFG_VALUE_OR_DEFAULT, "DP pdev LMAC ring")

#define CFG_DP_TIME_CONTROL_BP \
		CFG_INI_UINT("dp_time_control_bp", \
		WLAN_CFG_TIME_CONTROL_BP_MIN,\
		WLAN_CFG_TIME_CONTROL_BP_MAX,\
		WLAN_CFG_TIME_CONTROL_BP,\
		CFG_VALUE_OR_DEFAULT, "DP time control back pressure")

#ifdef CONFIG_SAWF_STATS
#define CFG_DP_SAWF_STATS \
		CFG_INI_UINT("dp_sawf_stats", \
		WLAN_CFG_SAWF_STATS_MIN,\
		WLAN_CFG_SAWF_STATS_MAX,\
		WLAN_CFG_SAWF_STATS,\
		CFG_VALUE_OR_DEFAULT, "DP sawf stats config")
#define CFG_DP_SAWF_STATS_CONFIG CFG(CFG_DP_SAWF_STATS)
#else
#define CFG_DP_SAWF_STATS_CONFIG
#endif

/*
 * <ini>
 * dp_rx_pending_hl_threshold - High threshold of frame number to start
 * frame dropping scheme
 * @Min: 0
 * @Max: 524288
 * @Default: 393216
 *
 * This ini entry is used to set a high limit threshold to start frame
 * dropping scheme
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_RX_PENDING_HL_THRESHOLD \
		CFG_INI_UINT("dp_rx_pending_hl_threshold", \
		WLAN_CFG_RX_PENDING_HL_THRESHOLD_MIN, \
		WLAN_CFG_RX_PENDING_HL_THRESHOLD_MAX, \
		WLAN_CFG_RX_PENDING_HL_THRESHOLD, \
		CFG_VALUE_OR_DEFAULT, "DP rx pending hl threshold")

/*
 * <ini>
 * dp_rx_pending_lo_threshold - Low threshold of frame number to stop
 * frame dropping scheme
 * @Min: 100
 * @Max: 524288
 * @Default: 393216
 *
 * This ini entry is used to set a low limit threshold to stop frame
 * dropping scheme
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_RX_PENDING_LO_THRESHOLD \
		CFG_INI_UINT("dp_rx_pending_lo_threshold", \
		WLAN_CFG_RX_PENDING_LO_THRESHOLD_MIN, \
		WLAN_CFG_RX_PENDING_LO_THRESHOLD_MAX, \
		WLAN_CFG_RX_PENDING_LO_THRESHOLD, \
		CFG_VALUE_OR_DEFAULT, "DP rx pending lo threshold")

#define CFG_DP_BASE_HW_MAC_ID \
		CFG_INI_UINT("dp_base_hw_macid", \
		0, 1, 1, \
		CFG_VALUE_OR_DEFAULT, "DP Base HW Mac ID")

#define CFG_DP_RX_HASH \
	CFG_INI_BOOL("dp_rx_hash", true, \
	"DP Rx Hash")

#define CFG_DP_TSO \
	CFG_INI_BOOL("TSOEnable", false, \
	"DP TSO Enabled")

#define CFG_DP_LRO \
	CFG_INI_BOOL("LROEnable", WLAN_LRO_ENABLE, \
	"DP LRO Enable")

#ifdef WLAN_USE_CONFIG_PARAMS
/*
 * <ini>
 * dp_tx_desc_use_512p - Use 512M tx descriptor size
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini entry is used as flag to use 512M tx descriptor size or not
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_TX_DESC_512P \
	CFG_INI_BOOL("dp_tx_desc_use_512p", false, \
	"DP TX DESC PINE SPECIFIC")

/*
 * <ini>
 * dp_nss_3radio_ring - Use 3 Radio NSS comp ring size
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini entry is used as flag to use 3 Radio NSS com ring size or not
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_NSS_3RADIO_RING \
	CFG_INI_BOOL("dp_nss_3radio_ring", false, \
	"DP NSS 3 RADIO RING SIZE")

/*
 * <ini>
 * dp_mon_ring_per_512M - Update monitor status ring as 512M profile
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini entry is used as flag to update monitor status ring as 512M profile
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_MON_STATUS_512M \
	CFG_INI_BOOL("dp_mon_ring_per_512M", false, \
	"DP MON STATUS RING SIZE PER 512M PROFILE")

/*
 * <ini>
 * dp_mon_2chain_ring - Reduce monitor rings size as for 2 Chains case
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini entry is used as flag to reduce monitor rings size as those used
 * in case of 2 Tx/RxChains
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_MON_2CHAIN_RING \
	CFG_INI_BOOL("dp_mon_2chain_ring", false, \
	"DP MON UPDATE RINGS FOR 2CHAIN")

/*
 * <ini>
 * dp_mon_4chain_ring - Update monitor rings size for 4 Chains case
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini entry is used as flag to reduce monitor rings size as those used
 * in case of 4 Tx/RxChains
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_MON_4CHAIN_RING \
	CFG_INI_BOOL("dp_mon_4chain_ring", false, \
	"DP MON UPDATE RINGS FOR 4CHAIN")

/*
 * <ini>
 * dp_4radip_rdp_reo - Update RDP REO map based on 4 radio config
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini entry is used as flag to update RDP reo map based on 4 Radio config
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_4RADIO_RDP_REO \
	CFG_INI_BOOL("dp_nss_4radio_rdp_reo", \
	false, "Update REO destination mapping for 4radio")

#define CFG_DP_INI_SECTION_PARAMS \
		CFG(CFG_DP_NSS_3RADIO_RING) \
		CFG(CFG_DP_TX_DESC_512P) \
		CFG(CFG_DP_MON_STATUS_512M) \
		CFG(CFG_DP_MON_2CHAIN_RING) \
		CFG(CFG_DP_MON_4CHAIN_RING) \
		CFG(CFG_DP_4RADIO_RDP_REO)
#else
#define CFG_DP_INI_SECTION_PARAMS
#endif

/*
 * <ini>
 * CFG_DP_SG - Enable the SG feature standalonely
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini entry is used to enable/disable SG feature standalonely.
 * Also does Rome support SG on TX, lithium does not.
 * For example the lithium does not support SG on UDP frames.
 * Which is able to handle SG only for TSO frames(in case TSO is enabled).
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_SG \
	CFG_INI_BOOL("dp_sg_support", false, \
	"DP SG Enable")

#define WLAN_CFG_GRO_ENABLE_MIN 0
#define WLAN_CFG_GRO_ENABLE_MAX 3
#define WLAN_CFG_GRO_ENABLE_DEFAULT 0
#define DP_GRO_ENABLE_BIT_SET     BIT(0)
#define DP_TC_BASED_DYNAMIC_GRO   BIT(1)

/*
 * <ini>
 * CFG_DP_GRO - Enable the GRO feature standalonely
 * @Min: 0
 * @Max: 3
 * @Default: 0
 *
 * This ini entry is used to enable/disable GRO feature standalonely.
 * Value 0: Disable GRO feature
 * Value 1: Enable GRO feature always
 * Value 3: Enable GRO dynamic feature where TC rule can control GRO
 *          behavior
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_GRO \
		CFG_INI_UINT("GROEnable", \
		WLAN_CFG_GRO_ENABLE_MIN, \
		WLAN_CFG_GRO_ENABLE_MAX, \
		WLAN_CFG_GRO_ENABLE_DEFAULT, \
		CFG_VALUE_OR_DEFAULT, "DP GRO Enable")

#define WLAN_CFG_TC_INGRESS_PRIO_MIN 0
#define WLAN_CFG_TC_INGRESS_PRIO_MAX 0xFFFF
#define WLAN_CFG_TC_INGRESS_PRIO_DEFAULT 0

#define CFG_DP_TC_INGRESS_PRIO \
		CFG_INI_UINT("tc_ingress_prio", \
		WLAN_CFG_TC_INGRESS_PRIO_MIN, \
		WLAN_CFG_TC_INGRESS_PRIO_MAX, \
		WLAN_CFG_TC_INGRESS_PRIO_DEFAULT, \
		CFG_VALUE_OR_DEFAULT, "DP tc ingress prio")

#define CFG_DP_OL_TX_CSUM \
	CFG_INI_BOOL("dp_offload_tx_csum_support", false, \
	"DP tx csum Enable")

#define CFG_DP_OL_RX_CSUM \
	CFG_INI_BOOL("dp_offload_rx_csum_support", false, \
	"DP rx csum Enable")

#define CFG_DP_RAWMODE \
	CFG_INI_BOOL("dp_rawmode_support", false, \
	"DP rawmode Enable")

#define CFG_DP_PEER_FLOW_CTRL \
	CFG_INI_BOOL("dp_peer_flow_control_support", false, \
	"DP peer flow ctrl Enable")

#define CFG_DP_NAPI \
	CFG_INI_BOOL("dp_napi_enabled", PLATFORM_VALUE(true, false), \
	"DP Napi Enabled")
/*
 * <ini>
 * gEnableP2pIpTcpUdpChecksumOffload - Enable checksum offload for P2P mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini entry is used to enable/disable TX checksum(UDP/TCP) for P2P modes.
 * This includes P2P device mode, P2P client mode and P2P GO mode.
 * The feature is enabled by default. To disable TX checksum for P2P, add the
 * following entry in ini file:
 * gEnableP2pIpTcpUdpChecksumOffload=0
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_P2P_TCP_UDP_CKSUM_OFFLOAD \
		CFG_INI_BOOL("gEnableP2pIpTcpUdpChecksumOffload", true, \
		"DP TCP UDP Checksum Offload for P2P mode (device/cli/go)")

/*
 * <ini>
 * gEnableNanIpTcpUdpChecksumOffload - Enable checksum offload for NAN mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_NAN_TCP_UDP_CKSUM_OFFLOAD \
		CFG_INI_BOOL("gEnableNanIpTcpUdpChecksumOffload", true, \
		"DP TCP UDP Checksum Offload for NAN mode")

/*
 * <ini>
 * gEnableIpTcpUdpChecksumOffload - Enable checksum offload
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DP_TCP_UDP_CKSUM_OFFLOAD \
	CFG_INI_BOOL("gEnableIpTcpUdpChecksumOffload", true, \
	"DP TCP UDP Checksum Offload")

#define CFG_DP_DEFRAG_TIMEOUT_CHECK \
	CFG_INI_BOOL("dp_defrag_timeout_check", true, \
	"DP Defrag Timeout Check")

#define CFG_DP_WBM_RELEASE_RING \
		CFG_INI_UINT("dp_wbm_release_ring", \
		WLAN_CFG_WBM_RELEASE_RING_SIZE_MIN, \
		WLAN_CFG_WBM_RELEASE_RING_SIZE_MAX, \
		WLAN_CFG_WBM_RELEASE_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP WBM Release Ring")

#define CFG_DP_TCL_CMD_CREDIT_RING \
		CFG_INI_UINT("dp_tcl_cmd_credit_ring", \
		WLAN_CFG_TCL_CMD_CREDIT_RING_SIZE_MIN, \
		WLAN_CFG_TCL_CMD_CREDIT_RING_SIZE_MAX, \
		WLAN_CFG_TCL_CMD_CREDIT_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP TCL Cmd_Credit ring")

#define CFG_DP_TCL_STATUS_RING \
		CFG_INI_UINT("dp_tcl_status_ring",\
		WLAN_CFG_TCL_STATUS_RING_SIZE_MIN, \
		WLAN_CFG_TCL_STATUS_RING_SIZE_MAX, \
		WLAN_CFG_TCL_STATUS_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP TCL status ring")

#define CFG_DP_REO_REINJECT_RING \
		CFG_INI_UINT("dp_reo_reinject_ring", \
		WLAN_CFG_REO_REINJECT_RING_SIZE_MIN, \
		WLAN_CFG_REO_REINJECT_RING_SIZE_MAX, \
		WLAN_CFG_REO_REINJECT_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP REO reinject ring")

#define CFG_DP_RX_RELEASE_RING \
		CFG_INI_UINT("dp_rx_release_ring", \
		WLAN_CFG_RX_RELEASE_RING_SIZE_MIN, \
		WLAN_CFG_RX_RELEASE_RING_SIZE_MAX, \
		WLAN_CFG_RX_RELEASE_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP Rx release ring")

#define CFG_DP_RX_DESTINATION_RING \
		CFG_INI_UINT("dp_reo_dst_ring", \
		WLAN_CFG_REO_DST_RING_SIZE_MIN, \
		WLAN_CFG_REO_DST_RING_SIZE_MAX, \
		WLAN_CFG_REO_DST_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP REO destination ring")

#define CFG_DP_REO_EXCEPTION_RING \
		CFG_INI_UINT("dp_reo_exception_ring", \
		WLAN_CFG_REO_EXCEPTION_RING_SIZE_MIN, \
		WLAN_CFG_REO_EXCEPTION_RING_SIZE_MAX, \
		WLAN_CFG_REO_EXCEPTION_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP REO exception ring")

#define CFG_DP_REO_CMD_RING \
		CFG_INI_UINT("dp_reo_cmd_ring", \
		WLAN_CFG_REO_CMD_RING_SIZE_MIN, \
		WLAN_CFG_REO_CMD_RING_SIZE_MAX, \
		WLAN_CFG_REO_CMD_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP REO command ring")

#define CFG_DP_REO_STATUS_RING \
		CFG_INI_UINT("dp_reo_status_ring", \
		WLAN_CFG_REO_STATUS_RING_SIZE_MIN, \
		WLAN_CFG_REO_STATUS_RING_SIZE_MAX, \
		WLAN_CFG_REO_STATUS_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP REO status ring")

#define CFG_DP_RXDMA_BUF_RING \
		CFG_INI_UINT("dp_rxdma_buf_ring", \
		WLAN_CFG_RXDMA_BUF_RING_SIZE_MIN, \
		WLAN_CFG_RXDMA_BUF_RING_SIZE_MAX, \
		WLAN_CFG_RXDMA_BUF_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP RXDMA buffer ring")

#define CFG_DP_RXDMA_REFILL_RING \
		CFG_INI_UINT("dp_rxdma_refill_ring", \
		WLAN_CFG_RXDMA_REFILL_RING_SIZE_MIN, \
		WLAN_CFG_RXDMA_REFILL_RING_SIZE_MAX, \
		WLAN_CFG_RXDMA_REFILL_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP RXDMA refilll ring")

#define CFG_DP_RXDMA_REFILL_LT_DISABLE \
	CFG_INI_BOOL("dp_disable_rx_buf_low_threshold", false, \
		     "Disable Low threshold interrupts for Rx Refill ring")

#define CFG_DP_TX_DESC_LIMIT_0 \
		CFG_INI_UINT("dp_tx_desc_limit_0", \
		WLAN_CFG_TX_DESC_LIMIT_0_MIN, \
		WLAN_CFG_TX_DESC_LIMIT_0_MAX, \
		WLAN_CFG_TX_DESC_LIMIT_0, \
		CFG_VALUE_OR_DEFAULT, "DP TX DESC limit 0")

#define CFG_DP_TX_DESC_LIMIT_1 \
		CFG_INI_UINT("dp_tx_desc_limit_1", \
		WLAN_CFG_TX_DESC_LIMIT_1_MIN, \
		WLAN_CFG_TX_DESC_LIMIT_1_MAX, \
		WLAN_CFG_TX_DESC_LIMIT_1, \
		CFG_VALUE_OR_DEFAULT, "DP TX DESC limit 1")

#define CFG_DP_TX_DESC_LIMIT_2 \
		CFG_INI_UINT("dp_tx_desc_limit_2", \
		WLAN_CFG_TX_DESC_LIMIT_2_MIN, \
		WLAN_CFG_TX_DESC_LIMIT_2_MAX, \
		WLAN_CFG_TX_DESC_LIMIT_2, \
		CFG_VALUE_OR_DEFAULT, "DP TX DESC limit 2")

#define CFG_DP_TX_DEVICE_LIMIT \
		CFG_INI_UINT("dp_tx_device_limit", \
		WLAN_CFG_TX_DEVICE_LIMIT_MIN, \
		WLAN_CFG_TX_DEVICE_LIMIT_MAX, \
		WLAN_CFG_TX_DEVICE_LIMIT, \
		CFG_VALUE_OR_DEFAULT, "DP TX DEVICE limit")

#define CFG_DP_TX_SPL_DEVICE_LIMIT \
		CFG_INI_UINT("dp_tx_spl_device_limit", \
		WLAN_CFG_TX_SPL_DEVICE_LIMIT_MIN, \
		WLAN_CFG_TX_SPL_DEVICE_LIMIT_MAX, \
		WLAN_CFG_TX_SPL_DEVICE_LIMIT, \
		CFG_VALUE_OR_DEFAULT, "DP TX Special DEVICE limit")

#define CFG_DP_TX_SW_INTERNODE_QUEUE \
		CFG_INI_UINT("dp_tx_sw_internode_queue", \
		WLAN_CFG_TX_SW_INTERNODE_QUEUE_MIN, \
		WLAN_CFG_TX_SW_INTERNODE_QUEUE_MAX, \
		WLAN_CFG_TX_SW_INTERNODE_QUEUE, \
		CFG_VALUE_OR_DEFAULT, "DP TX SW internode queue")

#define CFG_DP_RXDMA_MONITOR_BUF_RING \
		CFG_INI_UINT("dp_rxdma_monitor_buf_ring", \
		WLAN_CFG_RXDMA_MONITOR_BUF_RING_SIZE_MIN, \
		WLAN_CFG_RXDMA_MONITOR_BUF_RING_SIZE_MAX, \
		WLAN_CFG_RXDMA_MONITOR_BUF_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP RXDMA monitor buffer ring")

#define CFG_DP_TX_MONITOR_BUF_RING \
		CFG_INI_UINT("dp_tx_monitor_buf_ring", \
		WLAN_CFG_TX_MONITOR_BUF_RING_SIZE_MIN, \
		WLAN_CFG_TX_MONITOR_BUF_RING_SIZE_MAX, \
		WLAN_CFG_TX_MONITOR_BUF_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP TX monitor buffer ring")

#define CFG_DP_RXDMA_MONITOR_DST_RING \
		CFG_INI_UINT("dp_rxdma_monitor_dst_ring", \
		WLAN_CFG_RXDMA_MONITOR_DST_RING_SIZE_MIN, \
		WLAN_CFG_RXDMA_MONITOR_DST_RING_SIZE_MAX, \
		WLAN_CFG_RXDMA_MONITOR_DST_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP RXDMA monitor destination ring")

#define CFG_DP_TX_MONITOR_DST_RING \
		CFG_INI_UINT("dp_tx_monitor_dst_ring", \
		WLAN_CFG_TX_MONITOR_DST_RING_SIZE_MIN, \
		WLAN_CFG_TX_MONITOR_DST_RING_SIZE_MAX, \
		WLAN_CFG_TX_MONITOR_DST_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP TX monitor destination ring")

#define CFG_DP_RXDMA_MONITOR_STATUS_RING \
		CFG_INI_UINT("dp_rxdma_monitor_status_ring", \
		WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE_MIN, \
		WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE_MAX, \
		WLAN_CFG_RXDMA_MONITOR_STATUS_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP RXDMA monitor status ring")

#define CFG_DP_RXDMA_MONITOR_DESC_RING \
		CFG_INI_UINT("dp_rxdma_monitor_desc_ring", \
		WLAN_CFG_RXDMA_MONITOR_DESC_RING_SIZE_MIN, \
		WLAN_CFG_RXDMA_MONITOR_DESC_RING_SIZE_MAX, \
		WLAN_CFG_RXDMA_MONITOR_DESC_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP RXDMA monitor destination ring")

#define CFG_DP_RXDMA_ERR_DST_RING \
		CFG_INI_UINT("dp_rxdma_err_dst_ring", \
		WLAN_CFG_RXDMA_ERR_DST_RING_SIZE_MIN, \
		WLAN_CFG_RXDMA_ERR_DST_RING_SIZE_MAX, \
		WLAN_CFG_RXDMA_ERR_DST_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "RXDMA err destination ring")

#define CFG_DP_PER_PKT_LOGGING \
		CFG_INI_UINT("enable_verbose_debug", \
		0, 0xffff, 0, \
		CFG_VALUE_OR_DEFAULT, "Enable excessive per packet logging")

#define CFG_DP_TX_FLOW_START_QUEUE_OFFSET \
		CFG_INI_UINT("TxFlowStartQueueOffset", \
		0, 30, WLAN_CFG_TX_FLOW_START_QUEUE_OFFSET, \
		CFG_VALUE_OR_DEFAULT, "Start queue offset")

#define CFG_DP_TX_FLOW_STOP_QUEUE_TH \
		CFG_INI_UINT("TxFlowStopQueueThreshold", \
		0, 50, 15, \
		CFG_VALUE_OR_DEFAULT, "Stop queue Threshold")

#define CFG_DP_IPA_UC_TX_BUF_SIZE \
		CFG_INI_UINT("IpaUcTxBufSize", \
		0, 4096, WLAN_CFG_IPA_UC_TX_BUF_SIZE, \
		CFG_VALUE_OR_DEFAULT, "IPA tx buffer size")

#define CFG_DP_IPA_UC_TX_PARTITION_BASE \
		CFG_INI_UINT("IpaUcTxPartitionBase", \
		0, 9000, WLAN_CFG_IPA_UC_TX_PARTITION_BASE, \
		CFG_VALUE_OR_DEFAULT, "IPA tx partition base")

#define CFG_DP_IPA_UC_RX_IND_RING_COUNT \
		CFG_INI_UINT("IpaUcRxIndRingCount", \
		0, 2048, WLAN_CFG_IPA_UC_RX_IND_RING_COUNT, \
		CFG_VALUE_OR_DEFAULT, "IPA rx indication ring count")

#define CFG_DP_AP_STA_SECURITY_SEPERATION \
			CFG_INI_BOOL("gDisableIntraBssFwd", \
			false, "Disable intrs BSS Rx packets")

#define CFG_DP_ENABLE_DATA_STALL_DETECTION \
		CFG_INI_UINT("gEnableDataStallDetection", \
		0, 0xFFFFFFFF, 0x1, \
		CFG_VALUE_OR_DEFAULT, "Enable/Disable Data stall detection")

#define CFG_DP_RX_SW_DESC_WEIGHT \
		CFG_INI_UINT("dp_rx_sw_desc_weight", \
		WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE_MIN, \
		WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE_MAX, \
		WLAN_CFG_RX_SW_DESC_WEIGHT_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP RX SW DESC weight")

#define CFG_DP_RX_SW_DESC_NUM \
		CFG_INI_UINT("dp_rx_sw_desc_num", \
		WLAN_CFG_RX_SW_DESC_NUM_SIZE_MIN, \
		WLAN_CFG_RX_SW_DESC_NUM_SIZE_MAX, \
		WLAN_CFG_RX_SW_DESC_NUM_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP RX SW DESC num")

#define CFG_DP_RX_FLOW_SEARCH_TABLE_SIZE \
	CFG_INI_UINT("dp_rx_flow_search_table_size", \
		WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE_MIN, \
		WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE_MAX, \
		WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE_DEFAULT, \
		CFG_VALUE_OR_DEFAULT, \
		"DP Rx Flow Search Table Size in number of entries")

#define CFG_DP_RX_FLOW_TAG_ENABLE \
	CFG_INI_BOOL("dp_rx_flow_tag_enable", false, \
		     "Enable/Disable DP Rx Flow Tag")

#define CFG_DP_RX_FLOW_SEARCH_TABLE_PER_PDEV \
	CFG_INI_BOOL("dp_rx_per_pdev_flow_search", false, \
			"DP Rx Flow Search Table Is Per PDev")

#define CFG_DP_RX_MON_PROTOCOL_FLOW_TAG_ENABLE \
	CFG_INI_BOOL("dp_rx_monitor_protocol_flow_tag_enable", true, \
		     "Enable/Disable Rx Protocol & Flow tags in Monitor mode")

#define CFG_DP_TX_PER_PKT_VDEV_ID_CHECK \
	CFG_INI_BOOL("dp_tx_allow_per_pkt_vdev_id_check", false, \
		     "Enable/Disable tx Per Pkt vdev id check")

#define CFG_DP_HANDLE_INVALID_DECAP_TYPE_DISABLE \
	CFG_INI_BOOL("dp_handle_invalid_decap_type_disable", false, \
		     "Enable/Disable DP TLV out of order WAR")

#define CFG_DP_TXMON_SW_PEER_FILTERING \
	CFG_INI_BOOL("tx_litemon_sw_peer_filtering", false, \
		     "Enable SW based tx monitor peer fitlering")

#define CFG_DP_POINTER_TIMER_THRESHOLD_RX \
	CFG_INI_UINT("dp_rx_ptr_timer_threshold", \
	0, 0xFFFF, 0, \
	CFG_VALUE_OR_DEFAULT, "RX pointer update timer threshold")

#define CFG_DP_POINTER_NUM_THRESHOLD_RX \
	CFG_INI_UINT("dp_rx_ptr_num_threshold", \
	0, 63, 0, \
	CFG_VALUE_OR_DEFAULT, "RX pointer update entries number threshold")

/*
 * <ini>
 * dp_rx_fisa_enable - Control Rx datapath FISA
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable DP Rx FISA feature
 *
 * Related: dp_rx_flow_search_table_size
 *
 * Supported Feature: STA,P2P and SAP IPA disabled terminating
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_RX_FISA_ENABLE \
	CFG_INI_BOOL("dp_rx_fisa_enable", true, \
		     "Enable/Disable DP Rx FISA")

/*
 * <ini>
 * dp_rx_fisa_lru_del_enable - Control Rx datapath FISA
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable DP Rx FISA lru deletion feature
 *
 * Related: dp_rx_fisa_enable
 *
 * Supported Feature: STA,P2P and SAP IPA disabled terminating
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_RX_FISA_LRU_DEL_ENABLE \
	CFG_INI_BOOL("dp_rx_fisa_lru_del_enable", true, \
		     "Enable/Disable DP Rx FISA LRU deletion")

#define CFG_DP_RXDMA_MONITOR_RX_DROP_THRESHOLD \
		CFG_INI_UINT("mon_drop_thresh", \
		WLAN_CFG_RXDMA_MONITOR_RX_DROP_THRESH_SIZE_MIN, \
		WLAN_CFG_RXDMA_MONITOR_RX_DROP_THRESH_SIZE_MAX, \
		WLAN_CFG_RXDMA_MONITOR_RX_DROP_THRESH_SIZE, \
		CFG_VALUE_OR_DEFAULT, "RXDMA monitor rx drop threshold")

#define CFG_DP_PKTLOG_BUFFER_SIZE \
		CFG_INI_UINT("PktlogBufSize", \
		WLAN_CFG_PKTLOG_MIN_BUFFER_SIZE, \
		WLAN_CFG_PKTLOG_MAX_BUFFER_SIZE, \
		WLAN_CFG_PKTLOG_BUFFER_SIZE, \
		CFG_VALUE_OR_DEFAULT, "Packet Log buffer size")

#define CFG_DP_FULL_MON_MODE \
		CFG_INI_BOOL("full_mon_mode", \
		false, "Full Monitor mode support")

#define CFG_DP_REO_RINGS_MAP \
		CFG_INI_UINT("dp_reo_rings_map", \
		WLAN_CFG_NUM_REO_RINGS_MAP_MIN, \
		WLAN_CFG_NUM_REO_RINGS_MAP_MAX, \
		WLAN_CFG_NUM_REO_RINGS_MAP, \
		CFG_VALUE_OR_DEFAULT, "REO Destination Rings Mapping")

#define CFG_DP_RX_RADIO_0_DEFAULT_REO \
		CFG_INI_UINT("dp_rx_radio0_default_reo", \
		WLAN_CFG_RADIO_DEFAULT_REO_MIN, \
		WLAN_CFG_RADIO_DEFAULT_REO_MAX, \
		WLAN_CFG_RADIO_0_DEFAULT_REO, \
		CFG_VALUE_OR_DEFAULT, "Radio0 to REO destination default mapping")

#define CFG_DP_RX_RADIO_1_DEFAULT_REO \
		CFG_INI_UINT("dp_rx_radio1_default_reo", \
		WLAN_CFG_RADIO_DEFAULT_REO_MIN, \
		WLAN_CFG_RADIO_DEFAULT_REO_MAX, \
		WLAN_CFG_RADIO_1_DEFAULT_REO, \
		CFG_VALUE_OR_DEFAULT, "Radio1 to REO destination default mapping")

#define CFG_DP_RX_RADIO_2_DEFAULT_REO \
		CFG_INI_UINT("dp_rx_radio2_default_reo", \
		WLAN_CFG_RADIO_DEFAULT_REO_MIN, \
		WLAN_CFG_RADIO_DEFAULT_REO_MAX, \
		WLAN_CFG_RADIO_2_DEFAULT_REO, \
		CFG_VALUE_OR_DEFAULT, "Radio2 to REO destination default mapping")

#define CFG_DP_PEER_EXT_STATS \
		CFG_INI_BOOL("peer_ext_stats", \
		false, "Peer extended stats")

#define CFG_DP_PEER_JITTER_STATS \
		CFG_INI_BOOL("peer_jitter_stats", \
		false, "Peer Jitter stats")

#define CFG_DP_NAPI_SCALE_FACTOR \
		CFG_INI_UINT("dp_napi_scale_factor", \
		WLAN_CFG_DP_NAPI_SCALE_FACTOR_MIN, \
		WLAN_CFG_DP_NAPI_SCALE_FACTOR_MAX, \
		WLAN_CFG_DP_NAPI_SCALE_FACTOR, \
		CFG_VALUE_OR_DEFAULT, "NAPI scale factor for DP")

/*
 * <ini>
 * legacy_mode_csum_disable - Disable csum offload for legacy 802.11abg modes
 * @Min: 0
 * @Max: 1
 * @Default: Default value indicating if checksum should be disabled for
 * legacy WLAN modes
 *
 * This ini is used to disable HW checksum offload capability for legacy
 * connections
 *
 * Related: gEnableIpTcpUdpChecksumOffload should be enabled
 *
 * Usage: Internal
 *
 * </ini>
 */
#ifndef DP_LEGACY_MODE_CSM_DEFAULT_DISABLE
#define DP_LEGACY_MODE_CSM_DEFAULT_DISABLE 1
#endif

#define CFG_DP_LEGACY_MODE_CSUM_DISABLE \
	CFG_INI_BOOL("legacy_mode_csum_disable", \
		     DP_LEGACY_MODE_CSM_DEFAULT_DISABLE, \
		     "Enable/Disable legacy mode checksum")

#define CFG_DP_RX_BUFF_POOL_ENABLE \
	CFG_INI_BOOL("dp_rx_buff_prealloc_pool", false, \
		     "Enable/Disable DP RX emergency buffer pool support")

#define CFG_DP_RX_REFILL_BUFF_POOL_ENABLE \
	CFG_INI_BOOL("dp_rx_refill_buff_pool", false, \
		     "Enable/Disable DP RX refill buffer pool support")

#define CFG_DP_POLL_MODE_ENABLE \
		CFG_INI_BOOL("dp_poll_mode_enable", false, \
		"Enable/Disable Polling mode for data path")

#define CFG_DP_RX_FST_IN_CMEM \
	CFG_INI_BOOL("dp_rx_fst_in_cmem", false, \
		     "Enable/Disable flow search table in CMEM")
/*
 * <ini>
 * gEnableSWLM - Control DP Software latency manager
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable DP Software latency Manager
 *
 * Supported Feature: STA,P2P and SAP IPA disabled terminating
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_SWLM_ENABLE \
	CFG_INI_BOOL("gEnableSWLM", false, \
		     "Enable/Disable DP SWLM")
/*
 * <ini>
 * wow_check_rx_pending_enable - control to check RX frames pending in Wow
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to control DP Software to perform RX pending check
 * before entering WoW mode
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_WOW_CHECK_RX_PENDING \
		CFG_INI_BOOL("wow_check_rx_pending_enable", \
		false, \
		"enable rx frame pending check in WoW mode")
#define CFG_DP_DELAY_MON_REPLENISH \
		CFG_INI_BOOL("delay_mon_replenish", \
		true, "Delay Monitor Replenish")

#ifdef QCA_VDEV_STATS_HW_OFFLOAD_SUPPORT
#define WLAN_CFG_INT_VDEV_STATS_HW_OFFLOAD_TIMER_MIN 500
#define WLAN_CFG_INT_VDEV_STATS_HW_OFFLOAD_TIMER_MAX 2000
#define WLAN_CFG_INT_VDEV_STATS_HW_OFFLOAD_TIMER 500

#define CFG_DP_VDEV_STATS_HW_OFFLOAD_CONFIG \
		CFG_INI_BOOL("vdev_stats_hw_offload_config", \
		false, "Offload vdev stats to HW")
#define CFG_DP_VDEV_STATS_HW_OFFLOAD_TIMER \
		CFG_INI_UINT("vdev_stats_hw_offload_timer", \
		WLAN_CFG_INT_VDEV_STATS_HW_OFFLOAD_TIMER_MIN, \
		WLAN_CFG_INT_VDEV_STATS_HW_OFFLOAD_TIMER_MAX, \
		WLAN_CFG_INT_VDEV_STATS_HW_OFFLOAD_TIMER, \
		CFG_VALUE_OR_DEFAULT, \
		"vdev stats hw offload timer duration")
#define CFG_DP_VDEV_STATS_HW_OFFLOAD \
	CFG(CFG_DP_VDEV_STATS_HW_OFFLOAD_CONFIG) \
	CFG(CFG_DP_VDEV_STATS_HW_OFFLOAD_TIMER)
#else
#define CFG_DP_VDEV_STATS_HW_OFFLOAD
#endif

/*
 * <ini>
 * ghw_cc_enable - enable HW cookie conversion by register
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to control HW based 20 bits cookie to 64 bits
 * Desc virtual address conversion
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_HW_CC_ENABLE \
		CFG_INI_BOOL("ghw_cc_enable", \
		true, "Enable/Disable HW cookie conversion")

#ifdef IPA_OFFLOAD
/*
 * <ini>
 * dp_ipa_tx_ring_size - Set tcl ring size for IPA
 * @Min: 1024
 * @Max: 8096
 * @Default: 1024
 *
 * This ini sets the tcl ring size for IPA
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_IPA_TX_RING_SIZE \
		CFG_INI_UINT("dp_ipa_tx_ring_size", \
		WLAN_CFG_IPA_TX_RING_SIZE_MIN, \
		WLAN_CFG_IPA_TX_RING_SIZE_MAX, \
		WLAN_CFG_IPA_TX_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "IPA TCL ring size")

/*
 * <ini>
 * dp_ipa_tx_comp_ring_size - Set tx comp ring size for IPA
 * @Min: 1024
 * @Max: 8096
 * @Default: 1024
 *
 * This ini sets the tx comp ring size for IPA
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_IPA_TX_COMP_RING_SIZE \
		CFG_INI_UINT("dp_ipa_tx_comp_ring_size", \
		WLAN_CFG_IPA_TX_COMP_RING_SIZE_MIN, \
		WLAN_CFG_IPA_TX_COMP_RING_SIZE_MAX, \
		WLAN_CFG_IPA_TX_COMP_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "IPA tx comp ring size")

#ifdef IPA_WDI3_TX_TWO_PIPES
/*
 * <ini>
 * dp_ipa_tx_alt_ring_size - Set alt tcl ring size for IPA
 * @Min: 1024
 * @Max: 8096
 * @Default: 1024
 *
 * This ini sets the alt tcl ring size for IPA
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_IPA_TX_ALT_RING_SIZE \
		CFG_INI_UINT("dp_ipa_tx_alt_ring_size", \
		WLAN_CFG_IPA_TX_ALT_RING_SIZE_MIN, \
		WLAN_CFG_IPA_TX_ALT_RING_SIZE_MAX, \
		WLAN_CFG_IPA_TX_ALT_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, \
		"DP IPA TX Alternative Ring Size")

/*
 * <ini>
 * dp_ipa_tx_alt_comp_ring_size - Set tx alt comp ring size for IPA
 * @Min: 1024
 * @Max: 8096
 * @Default: 1024
 *
 * This ini sets the tx alt comp ring size for IPA
 *
 * Related: N/A
 *
 * Supported Feature: IPA
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_IPA_TX_ALT_COMP_RING_SIZE \
		CFG_INI_UINT("dp_ipa_tx_alt_comp_ring_size", \
		WLAN_CFG_IPA_TX_ALT_COMP_RING_SIZE_MIN, \
		WLAN_CFG_IPA_TX_ALT_COMP_RING_SIZE_MAX, \
		WLAN_CFG_IPA_TX_ALT_COMP_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, \
		"DP IPA TX Alternative Completion Ring Size")

#define CFG_DP_IPA_TX_ALT_RING_CFG \
		CFG(CFG_DP_IPA_TX_ALT_RING_SIZE) \
		CFG(CFG_DP_IPA_TX_ALT_COMP_RING_SIZE)

#else
#define CFG_DP_IPA_TX_ALT_RING_CFG
#endif

#define CFG_DP_IPA_TX_RING_CFG \
		CFG(CFG_DP_IPA_TX_RING_SIZE) \
		CFG(CFG_DP_IPA_TX_COMP_RING_SIZE)
#else
#define CFG_DP_IPA_TX_RING_CFG
#define CFG_DP_IPA_TX_ALT_RING_CFG
#endif

#ifdef WLAN_SUPPORT_PPEDS
#define WLAN_CFG_NUM_PPEDS_TX_DESC_MIN 16
#define WLAN_CFG_NUM_PPEDS_TX_DESC_MAX 0x8000
#define WLAN_CFG_NUM_PPEDS_TX_DESC 0x8000

#define WLAN_CFG_NUM_PPEDS_TX_CMP_NAPI_MIN 8
#define WLAN_CFG_NUM_PPEDS_TX_CMP_NAPI_MAX 256
#define WLAN_CFG_NUM_PPEDS_TX_CMP_NAPI 64

#define CFG_DP_PPEDS_TX_DESC \
		CFG_INI_UINT("dp_ppeds_tx_desc", \
		WLAN_CFG_NUM_PPEDS_TX_DESC_MIN, \
		WLAN_CFG_NUM_PPEDS_TX_DESC_MAX, \
		WLAN_CFG_NUM_PPEDS_TX_DESC, \
		CFG_VALUE_OR_DEFAULT, "DP PPEDS Tx Descriptors")

#define CFG_DP_PPEDS_TX_CMP_NAPI_BUDGET \
		CFG_INI_UINT("dp_ppeds_tx_cmp_napi_budget", \
		WLAN_CFG_NUM_PPEDS_TX_CMP_NAPI_MIN, \
		WLAN_CFG_NUM_PPEDS_TX_CMP_NAPI_MAX, \
		WLAN_CFG_NUM_PPEDS_TX_CMP_NAPI, \
		CFG_VALUE_OR_DEFAULT, "DP PPEDS Tx Comp handler napi budget")

#define CFG_DP_PPEDS_ENABLE \
	CFG_INI_BOOL("ppe_ds_enable", true, \
	"DP ppe enable flag")

#define CFG_DP_REO2PPE_RING \
		CFG_INI_UINT("dp_reo2ppe_ring", \
		WLAN_CFG_REO2PPE_RING_SIZE_MIN, \
		WLAN_CFG_REO2PPE_RING_SIZE_MAX, \
		WLAN_CFG_REO2PPE_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP REO2PPE ring")

#define CFG_DP_PPE2TCL_RING \
		CFG_INI_UINT("dp_ppe2tcl_ring", \
		WLAN_CFG_PPE2TCL_RING_SIZE_MIN, \
		WLAN_CFG_PPE2TCL_RING_SIZE_MAX, \
		WLAN_CFG_PPE2TCL_RING_SIZE, \
		CFG_VALUE_OR_DEFAULT, "DP PPE2TCL rings")

#define CFG_DP_PPEDS_CONFIG \
		CFG(CFG_DP_PPEDS_TX_CMP_NAPI_BUDGET) \
		CFG(CFG_DP_PPEDS_TX_DESC) \
		CFG(CFG_DP_PPEDS_ENABLE) \
		CFG(CFG_DP_REO2PPE_RING) \
		CFG(CFG_DP_PPE2TCL_RING)
#else
#define CFG_DP_PPEDS_CONFIG
#define WLAN_CFG_NUM_PPEDS_TX_DESC_MAX 0
#endif

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
/*
 * <ini>
 * dp_chip0_rx_ring_map - Set Rx ring map for CHIP 0
 * @Min: 0x0
 * @Max: 0xFF
 * @Default: 0xF
 *
 * This ini sets Rx ring map for CHIP 0
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_MLO_RX_RING_MAP \
		CFG_INI_UINT("dp_mlo_reo_rings_map", \
		WLAN_CFG_MLO_RX_RING_MAP_MIN, \
		WLAN_CFG_MLO_RX_RING_MAP_MAX, \
		WLAN_CFG_MLO_RX_RING_MAP, \
		CFG_VALUE_OR_DEFAULT, "DP MLO Rx ring map")


#define CFG_DP_MLO_CONFIG \
	CFG(CFG_DP_MLO_RX_RING_MAP)
#else
#define CFG_DP_MLO_CONFIG
#endif

/*
 * <ini>
 * dp_mpdu_retry_threshold_1 - threshold to increment mpdu success with retries
 * @Min: 0
 * @Max: 255
 * @Default: 0
 *
 * This ini entry is used to set first threshold to increment the value of
 * mpdu_success_with_retries
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_MPDU_RETRY_THRESHOLD_1 \
		CFG_INI_UINT("dp_mpdu_retry_threshold_1", \
		CFG_DP_MPDU_RETRY_THRESHOLD_MIN, \
		CFG_DP_MPDU_RETRY_THRESHOLD_MAX, \
		CFG_DP_MPDU_RETRY_THRESHOLD, \
		CFG_VALUE_OR_DEFAULT, "DP mpdu retry threshold 1")

/*
 * <ini>
 * dp_mpdu_retry_threshold_2 - threshold to increment mpdu success with retries
 * @Min: 0
 * @Max: 255
 * @Default: 0
 *
 * This ini entry is used to set second threshold to increment the value of
 * mpdu_success_with_retries
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_MPDU_RETRY_THRESHOLD_2 \
		CFG_INI_UINT("dp_mpdu_retry_threshold_2", \
		CFG_DP_MPDU_RETRY_THRESHOLD_MIN, \
		CFG_DP_MPDU_RETRY_THRESHOLD_MAX, \
		CFG_DP_MPDU_RETRY_THRESHOLD, \
		CFG_VALUE_OR_DEFAULT, "DP mpdu retry threshold 2")

#ifdef QCA_SUPPORT_TX_MIN_RATES_FOR_SPECIAL_FRAMES
/* Macro enabling support marking of notify frames by host */
#define DP_MARK_NOTIFY_FRAME_SUPPORT 1
#else
#define DP_MARK_NOTIFY_FRAME_SUPPORT 0
#endif /* QCA_SUPPORT_TX_MIN_RATES_FOR_SPECIAL_FRAMES */

/*
 * <ini>
 * Host DP AST entries database - Enable/Disable
 *
 * @Default: 0
 *
 * This ini enables/disables AST entries database on host
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DP_HOST_AST_DB_ENABLE \
	CFG_INI_BOOL("host_ast_db_enable", false, \
	"Host AST entries database Enable/Disable")

#ifdef DP_TX_PACKET_INSPECT_FOR_ILP
/*
 * <ini>
 * TX packet inspect for ILP - Enable/Disable
 *
 * @Default: true
 *
 * This ini enable/disables TX packet inspection for ILP feature
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_TX_PKT_INSPECT_FOR_ILP \
	CFG_INI_BOOL("tx_pkt_inspect_for_ilp", true, \
	"TX packet inspect for ILP")
#define CFG_TX_PKT_INSPECT_FOR_ILP_CFG CFG(CFG_TX_PKT_INSPECT_FOR_ILP)
#else
#define CFG_TX_PKT_INSPECT_FOR_ILP_CFG
#endif

#define CFG_DP \
		CFG(CFG_DP_HTT_PACKET_TYPE) \
		CFG(CFG_DP_INT_BATCH_THRESHOLD_OTHER) \
		CFG(CFG_DP_INT_BATCH_THRESHOLD_PPE2TCL) \
		CFG(CFG_DP_INT_BATCH_THRESHOLD_RX) \
		CFG(CFG_DP_INT_BATCH_THRESHOLD_TX) \
		CFG(CFG_DP_INT_TIMER_THRESHOLD_PPE2TCL) \
		CFG(CFG_DP_INT_TIMER_THRESHOLD_OTHER) \
		CFG(CFG_DP_INT_TIMER_THRESHOLD_RX) \
		CFG(CFG_DP_INT_TIMER_THRESHOLD_TX) \
		CFG(CFG_DP_MAX_ALLOC_SIZE) \
		CFG(CFG_DP_MAX_CLIENTS) \
		CFG(CFG_DP_MAX_PEER_ID) \
		CFG(CFG_DP_REO_DEST_RINGS) \
		CFG(CFG_DP_TX_COMP_RINGS) \
		CFG(CFG_DP_TCL_DATA_RINGS) \
		CFG(CFG_DP_NSS_REO_DEST_RINGS) \
		CFG(CFG_DP_NSS_TCL_DATA_RINGS) \
		CFG(CFG_DP_TX_DESC) \
		CFG(CFG_DP_TX_SPL_DESC) \
		CFG(CFG_DP_TX_EXT_DESC) \
		CFG(CFG_DP_TX_EXT_DESC_POOLS) \
		CFG(CFG_DP_PDEV_RX_RING) \
		CFG(CFG_DP_PDEV_TX_RING) \
		CFG(CFG_DP_RX_DEFRAG_TIMEOUT) \
		CFG(CFG_DP_TX_COMPL_RING_SIZE) \
		CFG(CFG_DP_TX_RING_SIZE) \
		CFG(CFG_DP_NSS_COMP_RING_SIZE) \
		CFG(CFG_DP_PDEV_LMAC_RING) \
		CFG(CFG_DP_TIME_CONTROL_BP) \
		CFG(CFG_DP_BASE_HW_MAC_ID) \
		CFG(CFG_DP_RX_HASH) \
		CFG(CFG_DP_TSO) \
		CFG(CFG_DP_LRO) \
		CFG(CFG_DP_SG) \
		CFG(CFG_DP_GRO) \
		CFG(CFG_DP_TC_INGRESS_PRIO) \
		CFG(CFG_DP_OL_TX_CSUM) \
		CFG(CFG_DP_OL_RX_CSUM) \
		CFG(CFG_DP_RAWMODE) \
		CFG(CFG_DP_PEER_FLOW_CTRL) \
		CFG(CFG_DP_NAPI) \
		CFG(CFG_DP_TCP_UDP_CKSUM_OFFLOAD) \
		CFG(CFG_DP_NAN_TCP_UDP_CKSUM_OFFLOAD) \
		CFG(CFG_DP_P2P_TCP_UDP_CKSUM_OFFLOAD) \
		CFG(CFG_DP_DEFRAG_TIMEOUT_CHECK) \
		CFG(CFG_DP_WBM_RELEASE_RING) \
		CFG(CFG_DP_TCL_CMD_CREDIT_RING) \
		CFG(CFG_DP_TCL_STATUS_RING) \
		CFG(CFG_DP_REO_REINJECT_RING) \
		CFG(CFG_DP_RX_RELEASE_RING) \
		CFG(CFG_DP_REO_EXCEPTION_RING) \
		CFG(CFG_DP_RX_DESTINATION_RING) \
		CFG(CFG_DP_REO_CMD_RING) \
		CFG(CFG_DP_REO_STATUS_RING) \
		CFG(CFG_DP_RXDMA_BUF_RING) \
		CFG(CFG_DP_RXDMA_REFILL_RING) \
		CFG(CFG_DP_RXDMA_REFILL_LT_DISABLE) \
		CFG(CFG_DP_TX_DESC_LIMIT_0) \
		CFG(CFG_DP_TX_DESC_LIMIT_1) \
		CFG(CFG_DP_TX_DESC_LIMIT_2) \
		CFG(CFG_DP_TX_DEVICE_LIMIT) \
		CFG(CFG_DP_TX_SPL_DEVICE_LIMIT) \
		CFG(CFG_DP_TX_SW_INTERNODE_QUEUE) \
		CFG(CFG_DP_RXDMA_MONITOR_BUF_RING) \
		CFG(CFG_DP_RXDMA_MONITOR_DST_RING) \
		CFG(CFG_DP_RXDMA_MONITOR_STATUS_RING) \
		CFG(CFG_DP_RXDMA_MONITOR_DESC_RING) \
		CFG(CFG_DP_RXDMA_ERR_DST_RING) \
		CFG(CFG_DP_PER_PKT_LOGGING) \
		CFG(CFG_DP_TX_FLOW_START_QUEUE_OFFSET) \
		CFG(CFG_DP_TX_FLOW_STOP_QUEUE_TH) \
		CFG(CFG_DP_IPA_UC_TX_BUF_SIZE) \
		CFG(CFG_DP_IPA_UC_TX_PARTITION_BASE) \
		CFG(CFG_DP_IPA_UC_RX_IND_RING_COUNT) \
		CFG(CFG_DP_AP_STA_SECURITY_SEPERATION) \
		CFG(CFG_DP_ENABLE_DATA_STALL_DETECTION) \
		CFG(CFG_DP_RX_SW_DESC_WEIGHT) \
		CFG(CFG_DP_RX_SW_DESC_NUM) \
		CFG(CFG_DP_RX_FLOW_SEARCH_TABLE_SIZE) \
		CFG(CFG_DP_RX_FLOW_TAG_ENABLE) \
		CFG(CFG_DP_RX_FLOW_SEARCH_TABLE_PER_PDEV) \
		CFG(CFG_DP_RX_MON_PROTOCOL_FLOW_TAG_ENABLE) \
		CFG(CFG_DP_RXDMA_MONITOR_RX_DROP_THRESHOLD) \
		CFG(CFG_DP_PKTLOG_BUFFER_SIZE) \
		CFG(CFG_DP_RX_FISA_ENABLE) \
		CFG(CFG_DP_RX_FISA_LRU_DEL_ENABLE) \
		CFG(CFG_DP_FULL_MON_MODE) \
		CFG(CFG_DP_REO_RINGS_MAP) \
		CFG(CFG_DP_PEER_EXT_STATS) \
		CFG(CFG_DP_PEER_JITTER_STATS) \
		CFG(CFG_DP_RX_BUFF_POOL_ENABLE) \
		CFG(CFG_DP_RX_REFILL_BUFF_POOL_ENABLE) \
		CFG(CFG_DP_RX_PENDING_HL_THRESHOLD) \
		CFG(CFG_DP_RX_PENDING_LO_THRESHOLD) \
		CFG(CFG_DP_LEGACY_MODE_CSUM_DISABLE) \
		CFG(CFG_DP_POLL_MODE_ENABLE) \
		CFG(CFG_DP_SWLM_ENABLE) \
		CFG(CFG_DP_TX_PER_PKT_VDEV_ID_CHECK) \
		CFG(CFG_DP_RX_FST_IN_CMEM) \
		CFG(CFG_DP_RX_RADIO_0_DEFAULT_REO) \
		CFG(CFG_DP_RX_RADIO_1_DEFAULT_REO) \
		CFG(CFG_DP_RX_RADIO_2_DEFAULT_REO) \
		CFG(CFG_DP_WOW_CHECK_RX_PENDING) \
		CFG(CFG_DP_HW_CC_ENABLE) \
		CFG(CFG_DP_DELAY_MON_REPLENISH) \
		CFG(CFG_DP_TX_MONITOR_BUF_RING) \
		CFG(CFG_DP_TX_MONITOR_DST_RING) \
		CFG(CFG_DP_MPDU_RETRY_THRESHOLD_1) \
		CFG(CFG_DP_MPDU_RETRY_THRESHOLD_2) \
		CFG_DP_IPA_TX_RING_CFG \
		CFG_DP_PPEDS_CONFIG \
		CFG_DP_IPA_TX_ALT_RING_CFG \
		CFG_DP_MLO_CONFIG \
		CFG_DP_INI_SECTION_PARAMS \
		CFG_DP_VDEV_STATS_HW_OFFLOAD \
		CFG(CFG_DP_TX_CAPT_MAX_MEM_MB) \
		CFG(CFG_DP_NAPI_SCALE_FACTOR) \
		CFG(CFG_DP_HOST_AST_DB_ENABLE) \
		CFG_DP_SAWF_STATS_CONFIG \
		CFG(CFG_DP_HANDLE_INVALID_DECAP_TYPE_DISABLE) \
		CFG(CFG_DP_TXMON_SW_PEER_FILTERING) \
		CFG_TX_PKT_INSPECT_FOR_ILP_CFG \
		CFG(CFG_DP_POINTER_TIMER_THRESHOLD_RX) \
		CFG(CFG_DP_POINTER_NUM_THRESHOLD_RX)
#endif /* _CFG_DP_H_ */

/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/**
 * DOC: This file contains chipset stats implementstion
 */

#ifndef __WLAN_CP_STATS_CHIPSET_STATS__
#define __WLAN_CP_STATS_CHIPSET_STATS__

#include <wlan_cmn.h>
#include <qdf_status.h>
#include <qdf_trace.h>
#include "wlan_cp_stats_chipset_stats_events.h"

#define MAX_CSTATS_NODE_LENGTH 2048
#define MAX_CSTATS_NODE_COUNT 256
#define MAX_CSTATS_VERSION_BUFF_LENGTH 100

#define CSTATS_QMI_EVENT_TYPE 1

#define ANI_NL_MSG_CSTATS_HOST_LOG_TYPE 110
#define ANI_NL_MSG_CSTATS_FW_LOG_TYPE 111

#define CSTATS_MARKER_SZ 6
#define CSTATS_HOST_START_MARKER "CS_HSM"
#define CSTATS_HOST_END_MARKER "CS_HEM"
#define CSTATS_FW_START_MARKER "CS_FSM"
#define CSTATS_FW_END_MARKER  "CS_FEM"

#ifdef QDF_LITTLE_ENDIAN_MACHINE
#define CHIPSET_STATS_MACHINE_ENDIANNESS (0)
#else
#define CHIPSET_STATS_MACHINE_ENDIANNESS (1)
#endif

#define CSTATS_SET_BIT(value, mask) ((value) |= (1 << (mask)))

#define CSTATS_MAC_COPY(to, from) \
	do {\
		to[0] = from[0]; \
		to[1] = from[1]; \
		to[2] = from[2]; \
		to[3] = from[5]; \
	} while (0)

/**
 * enum cstats_types - Types of chipset stats
 * @CSTATS_HOST_TYPE : Host related chipset stats
 * @CSTATS_FW_TYPE : Firmware related chipset stats
 * @CSTATS_MAX_TYPE : Invalid
 */
enum cstats_types {
	CSTATS_HOST_TYPE,
	CSTATS_FW_TYPE,
	CSTATS_MAX_TYPE,
};

struct cstats_tx_rx_ops {
	int (*cstats_send_data_to_usr)(char *buff, unsigned int len,
				       enum cstats_types type);
};

struct cstats_node {
	qdf_list_node_t node;
	unsigned int radio;
	unsigned int index;
	unsigned int filled_length;
	char logbuf[MAX_CSTATS_NODE_LENGTH];
};

struct chipset_stats {
	qdf_list_t cstat_free_list[CSTATS_MAX_TYPE];
	qdf_list_t cstat_filled_list[CSTATS_MAX_TYPE];

	/* Lock to synchronize access to shared cstats resource */
	qdf_spinlock_t cstats_lock[CSTATS_MAX_TYPE];
	struct cstats_node *ccur_node[CSTATS_MAX_TYPE];
	unsigned int cstat_drop_cnt[CSTATS_MAX_TYPE];

	/* Dont move filled list nodes to free list after flush to user space */
	bool cstats_no_flush[CSTATS_MAX_TYPE];
	struct cstats_tx_rx_ops ops;
};

#define wlan_cstats_fw_stats(len, buf) \
	wlan_cp_stats_cstats_write_to_buff(CSTATS_FW_TYPE, buf, len)
#define wlan_cstats_host_stats(len, buf) \
	wlan_cp_stats_cstats_write_to_buff(CSTATS_HOST_TYPE, buf, len)

#ifdef WLAN_CHIPSET_STATS
/**
 * wlan_cp_stats_cstats_init() - Initialize chipset stats infra
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_cp_stats_cstats_init(void);

/**
 * wlan_cp_stats_cstats_deinit() - Deinitialize chipset stats infra
 *
 * Return: void
 */
void wlan_cp_stats_cstats_deinit(void);

/**
 * wlan_cp_stats_cstats_register_tx_rx_ops() - Register chipset stats ops
 *
 * @ops : tx rx ops
 *
 * Return: void
 */
void wlan_cp_stats_cstats_register_tx_rx_ops(struct cstats_tx_rx_ops *ops);

/*
 * wlan_cp_stats_cstats_write_to_buff() - Write stats to the chipset stats
 * buffer
 * @type: Type of chipset stats to be written
 * @to_be_sent: Pointer to stats payload which is to be write to cstats buffer
 * @length: Length of the payload
 *
 * Return : void
 */
void wlan_cp_stats_cstats_write_to_buff(enum cstats_types type,
					void *to_be_sent,
					uint32_t length);

/**
 * wlan_cp_stats_cstats_send_buffer_to_user() - Flush chipset stats to the
 * middleware
 * @type: Type of chipset stats to be sent
 *
 * Return : 0 on success and errno on failure
 */
int wlan_cp_stats_cstats_send_buffer_to_user(enum cstats_types type);

/*
 * wlan_cp_stats_cstats_pkt_log() - Data packets stats
 * @sa - Source addr
 * @da - Destination addr
 * @pkt_type - Packet type
 * @subtype - Subtype
 * @dir - Direction
 * @status - Status
 * @vdev_id - Vdev iD
 * @op_mode - opmode
 *
 * Return : void
 */
void wlan_cp_stats_cstats_pkt_log(uint8_t *sa, uint8_t *da,
				  enum qdf_proto_type pkt_type,
				  enum qdf_proto_subtype subtype,
				  enum qdf_proto_dir dir,
				  enum qdf_dp_tx_rx_status status,
				  uint8_t vdev_id, enum QDF_OPMODE op_mode);
#else
static inline QDF_STATUS wlan_cp_stats_cstats_init(void)
{
	return 0;
}

static inline void wlan_cp_stats_cstats_deinit(void)
{
}

static inline void
wlan_cp_stats_cstats_register_tx_rx_ops(struct cstats_tx_rx_ops *ops)
{
}

static inline void
wlan_cp_stats_cstats_write_to_buff(enum cstats_types type, void *to_be_sent,
				   uint32_t length)
{
}

static inline int
wlan_cp_stats_cstats_send_buffer_to_user(enum cstats_types type)
{
	return 0;
}
#endif /* WLAN_CHIPSET_STATS */
#endif /* __WLAN_CP_STATS_CHIPSET_STATS__ */

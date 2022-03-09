/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/spinlock.h>
#include <net/rmnet_config.h>

#ifndef _RMNET_MAP_H_
#define _RMNET_MAP_H_

struct rmnet_map_control_command_s {
	u8  command_name;
#ifndef RMNET_USE_BIG_ENDIAN_STRUCTS
	u8  cmd_type:2;
	u8  reserved:6;
#else
	u8  reserved:6;
	u8  cmd_type:2;
#endif /* RMNET_USE_BIG_ENDIAN_STRUCTS */
	u16 reserved2;
	u32 transaction_id;
	union {
		u8  data[65528];
		struct {
#ifndef RMNET_USE_BIG_ENDIAN_STRUCTS
			u16  ip_family:2;
			u16  reserved:14;
#else
			u16  reserved:14;
			u16  ip_family:2;
#endif /* RMNET_USE_BIG_ENDIAN_STRUCTS */
			u16  flow_control_seq_num;
			u32  qos_id;
		} flow_control;
	};
}  __aligned(1);

struct rmnet_map_dl_checksum_trailer_s {
	unsigned char  reserved_h;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char  valid:1;
	unsigned char  reserved_l:7;
#elif defined(__BIG_ENDIAN_BITFIELD)
	unsigned char  reserved_l:7;
	unsigned char  valid:1;
#else
#error "Please fix <asm/byteorder.h>"
#endif
	unsigned short checksum_start_offset;
	unsigned short checksum_length;
	unsigned short checksum_value;
} __aligned(1);

struct rmnet_map_ul_checksum_header_s {
	unsigned short checksum_start_offset;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned short checksum_insert_offset:14;
	unsigned short udp_ip4_ind:1;
	unsigned short cks_en:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	unsigned short cks_en:1;
	unsigned short udp_ip4_ind:1;
	unsigned short checksum_insert_offset:14;
#else
#error "Please fix <asm/byteorder.h>"
#endif
} __aligned(1);

enum rmnet_map_results_e {
	RMNET_MAP_SUCCESS,
	RMNET_MAP_CONSUMED,
	RMNET_MAP_GENERAL_FAILURE,
	RMNET_MAP_NOT_ENABLED,
	RMNET_MAP_FAILED_AGGREGATION,
	RMNET_MAP_FAILED_MUX
};

enum rmnet_map_mux_errors_e {
	RMNET_MAP_MUX_SUCCESS,
	RMNET_MAP_MUX_INVALID_MUX_ID,
	RMNET_MAP_MUX_INVALID_PAD_LENGTH,
	RMNET_MAP_MUX_INVALID_PKT_LENGTH,
	/* This should always be the last element */
	RMNET_MAP_MUX_ENUM_LENGTH
};

enum rmnet_map_checksum_errors_e {
	RMNET_MAP_CHECKSUM_OK,
	RMNET_MAP_CHECKSUM_VALID_FLAG_NOT_SET,
	RMNET_MAP_CHECKSUM_VALIDATION_FAILED,
	RMNET_MAP_CHECKSUM_ERR_UNKNOWN,
	RMNET_MAP_CHECKSUM_ERR_NOT_DATA_PACKET,
	RMNET_MAP_CHECKSUM_ERR_BAD_BUFFER,
	RMNET_MAP_CHECKSUM_ERR_UNKNOWN_IP_VERSION,
	RMNET_MAP_CHECKSUM_ERR_UNKNOWN_TRANSPORT,
	RMNET_MAP_CHECKSUM_FRAGMENTED_PACKET,
	RMNET_MAP_CHECKSUM_SKIPPED,
	RMNET_MAP_CHECKSUM_SW,
	/* This should always be the last element */
	RMNET_MAP_CHECKSUM_ENUM_LENGTH
};

enum rmnet_map_commands_e {
	RMNET_MAP_COMMAND_NONE,
	RMNET_MAP_COMMAND_FLOW_DISABLE,
	RMNET_MAP_COMMAND_FLOW_ENABLE,
	/* These should always be the last 2 elements */
	RMNET_MAP_COMMAND_UNKNOWN,
	RMNET_MAP_COMMAND_ENUM_LENGTH
};

enum rmnet_map_agg_state_e {
	RMNET_MAP_AGG_IDLE,
	RMNET_MAP_TXFER_SCHEDULED
};

#define RMNET_MAP_COMMAND_REQUEST     0
#define RMNET_MAP_COMMAND_ACK         1
#define RMNET_MAP_COMMAND_UNSUPPORTED 2
#define RMNET_MAP_COMMAND_INVALID     3

#define RMNET_MAP_NO_PAD_BYTES        0
#define RMNET_MAP_ADD_PAD_BYTES       1

uint8_t rmnet_map_demultiplex(struct sk_buff *skb);
struct sk_buff *rmnet_map_deaggregate(struct sk_buff *skb,
				      struct rmnet_phys_ep_config *config);

struct rmnet_map_header_s *rmnet_map_add_map_header(struct sk_buff *skb,
						    int hdrlen, int pad);
rx_handler_result_t rmnet_map_command(struct sk_buff *skb,
				      struct rmnet_phys_ep_config *config);
void rmnet_map_aggregate(struct sk_buff *skb,
			 struct rmnet_phys_ep_config *config);

int rmnet_map_checksum_downlink_packet(struct sk_buff *skb);
int rmnet_map_checksum_uplink_packet(struct sk_buff *skb,
				     struct net_device *orig_dev,
				     u32 egress_data_format);
int rmnet_ul_aggregation_skip(struct sk_buff *skb, int offset);
enum hrtimer_restart rmnet_map_flush_packet_queue(struct hrtimer *t);
#endif /* _RMNET_MAP_H_ */

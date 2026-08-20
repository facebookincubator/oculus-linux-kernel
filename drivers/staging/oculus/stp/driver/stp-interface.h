/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __STP_INTERFACE_H__
#define __STP_INTERFACE_H__

#include <linux/printk.h>
#include <uapi/linux/stp_interface_abi.h>

#define STP_INTERFACE_DRV_NAME "stp_interface"
#define STP_INTERFACE_WAIT_FOR_CHANNEL_TIMEOUT_MSEC 5000
#define STP_CHANNEL_RETRY_DELAY_MSEC 1000
#define STP_CHANNEL_READ_WAIT_TIMEOUT_MSEC 1000
#define STP_CHANNEL_OPEN_WORK_RETRIES 10
#define STP_MAX_PROBE_RETRIES 5
#define STP_MAX_CONSUME 16
#define STP_INTERFACE_BAD_PACKET_PATTERN 0xdeadbeef

typedef struct stp_interface stp_interface;

int stp_interface_xfer(struct device_node *stp_interface_node, uint8_t *buf,
		       struct stp_interface_msg_header *header,
		       struct stp_interface_msg_payload *payload,
		       uint8_t stp_msg_len, uint8_t max_message_size,
		       uint16_t magic);

#endif

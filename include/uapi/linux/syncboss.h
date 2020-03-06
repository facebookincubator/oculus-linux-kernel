#ifndef _UAPI_LINUX_SYNCBOSS_H
#define _UAPI_LINUX_SYNCBOSS_H

#include <linux/major.h>
#include <linux/types.h>

#define SYNCBOSS_MAX_TRANSACTION_LENGTH 512
#define SYNCBOSS_DRIVER_HEADER_VERSION_V1 1

// An element in our history of received data from the syncboss.
struct rx_history_elem {
	// A counter that increases with each transaction that is made
	uint64_t rx_ctr;
	// Timestamp of the start of the transaction
	uint64_t transaction_start_time_ns;
	// Timestamp of the end of the transaction
	uint64_t transaction_end_time_ns;
	// Number of bytes stored in buf
	uint8_t transaction_length;
	// Transaction rx data
	uint8_t buf[SYNCBOSS_MAX_TRANSACTION_LENGTH];
} __attribute__((packed));

struct syncboss_driver_data_header_t {
	uint8_t header_version;
	uint8_t header_length;
	bool from_driver;
} __attribute__((packed));

struct syncboss_driver_data_header_driver_message_t {
	struct syncboss_driver_data_header_t header;
	uint32_t driver_message_type;
	uint32_t driver_message_data;
} __attribute__((packed));

/* Max number of stream events to filter */
#define SYNCBOSS_MAX_FILTERED_TYPES 16

/* Struct passed to SYNCBOSS_SET_STREAMFILTER_IOCTL which is used to
 * specify the set of desired stream event types a client is
 * interested in.
 */
struct syncboss_driver_stream_type_filter {
	uint8_t selected_types[SYNCBOSS_MAX_FILTERED_TYPES];
	uint8_t num_selected;
} __attribute__((packed));

struct syncboss_nsync_event {
	uint64_t timestamp;
	uint64_t count;
} __attribute__((packed));

#define SYNCBOSS_DRIVER_MESSAGE_WAKEUP 1
#define SYNCBOSS_DRIVER_MESSAGE_PROX_MSG 2

#define SYNCBOSS_PROX_EVENT_SYSTEM_UP 0
#define SYNCBOSS_PROX_EVENT_SYSTEM_DOWN 1
#define SYNCBOSS_PROX_EVENT_PROX_ON 2
#define SYNCBOSS_PROX_EVENT_PROX_OFF 3

/* ioctl used to set per-client stream filter */
#define SYNCBOSS_SET_STREAMFILTER_IOCTL \
	_IOW(MISC_MAJOR, 1, struct syncboss_driver_stream_type_filter)

 #endif

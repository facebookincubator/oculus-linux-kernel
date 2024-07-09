#ifndef _UAPI_LINUX_SYNCBOSS_H
#define _UAPI_LINUX_SYNCBOSS_H

#include <linux/major.h>
#include <linux/types.h>

#define SYNCBOSS_MAX_TRANSACTION_LENGTH 512

/**
 * Size of the maximum data packet expected to be transferred by this driver
 */
#define SYNCBOSS_MAX_DATA_PKT_SIZE 255

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

enum syncboss_time_offset_status {
	SYNCBOSS_TIME_OFFSET_INVALID = 0,
	SYNCBOSS_TIME_OFFSET_VALID,
	SYNCBOSS_TIME_OFFSET_ERROR,
};

/*
 * All syncboss_driver_data_header_v* structs are assumed by userspace
 * to start with uint8_t header_version and uint8_t header_length.
 */
struct syncboss_driver_data_header_v2_t {
	uint8_t header_version;
	uint8_t header_length;
	bool from_driver;
	uint8_t nsync_offset_status; // See enum syncboss_time_offset_status
	int64_t nsync_offset_us;
} __attribute__((packed));

struct syncboss_driver_data_header_v3_t {
	uint8_t header_version;
	uint8_t header_length;
	bool from_driver;
	uint8_t nsync_offset_status; // See enum syncboss_time_offset_status
	int64_t nsync_offset_us;
	uint8_t remote_offset_status; // See enum syncboss_time_offset_status
	int64_t remote_offset_us;
} __attribute__((packed));

struct syncboss_driver_data_header_driver_message_v2_t {
	struct syncboss_driver_data_header_v2_t header;
	uint32_t driver_message_type;
	uint32_t driver_message_data;
} __attribute__((packed));

struct syncboss_driver_data_header_driver_message_v3_t {
	struct syncboss_driver_data_header_v3_t header;
	uint32_t driver_message_type;
	uint32_t driver_message_data;
} __attribute__((packed));

struct uapi_pkt_v2_t {
	struct syncboss_driver_data_header_v2_t header;
	uint8_t payload[SYNCBOSS_MAX_DATA_PKT_SIZE];
} __attribute__((packed));

struct uapi_pkt_v3_t {
	struct syncboss_driver_data_header_v3_t header;
	uint8_t payload[SYNCBOSS_MAX_DATA_PKT_SIZE];
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
	uint32_t count;
	uint8_t offset_valid;
	int64_t offset_us;
} __attribute__((packed));
#define SYNCBOSS_NSYNC_FRAME_MESSAGE_TYPE 27

struct syncboss_display_event {
	uint64_t timestamp;
	uint32_t count;
} __attribute__((packed));
#define SYNCBOSS_DISPLAY_FRAME_MESSAGE_TYPE 85

/*
 * THIS MUST BE ALINGED TO LIBSYNCBOSS Struct used in fbsource hal library -
 * fbsource/arvr/firmware/projects/libsyncboss/os_interface/syncboss_hal_impl_android_driver.c
 */
struct syncboss_driver_directchannel_shared_memory_config {
	int dmabuf_fd;
	size_t direct_channel_buffer_size; /* must be less than dma_buf_size */
	uint8_t uapi_pkt_type;
	uint8_t wake_epoll; /* set to true to wake up epoll when data arrives, false otherwise. */
} __attribute__((packed));
/*
 * THIS MUST BE ALINGED TO LIBSYNCBOSS Struct used in fbsource hal library -
 * fbsource/arvr/firmware/projects/libsyncboss/os_interface/syncboss_hal_impl_android_driver.c
 */
struct syncboss_driver_directchannel_shared_memory_clear_config {
	int dmabuf_fd;
	uint8_t uapi_pkt_type;
} __attribute__((packed));

/*
 * Direct Channel Format defined by Android
 */
union syncboss_direct_channel_payload {
	float fdata[16];
	int64_t idata[8];
	uint8_t data[64];
};

/* Direct Channel defintion for android */
struct syncboss_sensor_direct_channel_data {
	int32_t		size;
	int32_t		report_token;
	int32_t		sensor_type;
	uint32_t	counter;
	int64_t		timestamp;
	union syncboss_direct_channel_payload payload;
	int32_t		reserved[4];
} __attribute__((packed));

#define SYNCBOSS_DRIVER_MESSAGE_POWERSTATE_MSG 2

#define SYNCBOSS_PROX_EVENT_SYSTEM_UP 0
#define SYNCBOSS_PROX_EVENT_SYSTEM_DOWN 1
#define SYNCBOSS_PROX_EVENT_PROX_ON 2
#define SYNCBOSS_PROX_EVENT_PROX_OFF 3

/* ioctl used to set per-client stream filter */
#define SYNCBOSS_SET_STREAMFILTER_IOCTL \
	_IOW(MISC_MAJOR, 1, struct syncboss_driver_stream_type_filter)

/* ioctl used to allocate and release sequence numbers */
#define SYNCBOSS_SEQUENCE_NUMBER_ALLOCATE_IOCTL _IOR(MISC_MAJOR, 2, uint8_t)
#define SYNCBOSS_SEQUENCE_NUMBER_RELEASE_IOCTL _IOW(MISC_MAJOR, 3, uint8_t)
/* Ioctl used to set dma buf for a channel */
#define SYNCBOSS_SET_DIRECTCHANNEL_SHARED_MEMORY_IOCTL \
_IOW(MISC_MAJOR, 4, struct syncboss_driver_directchannel_shared_memory_config)
#define SYNCBOSS_CLEAR_DIRECTCHANNEL_SHARED_MEMORY_IOCTL \
_IOW(MISC_MAJOR, 5, struct syncboss_driver_directchannel_shared_memory_clear_config)
 #endif

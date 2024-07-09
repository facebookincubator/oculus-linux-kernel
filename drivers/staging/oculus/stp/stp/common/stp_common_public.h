/* SPDX-License-Identifier: GPL-2.0 */
#ifndef STP_COMMON_PUBLIC_H
#define STP_COMMON_PUBLIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stp/common/stp_os.h>

// total number of channels
#define STP_TOTAL_NUM_CHANNELS 32

// total size of a data packet
#define STP_TOTAL_DATA_SIZE 256

/* STP error codes */
enum stp_error_type {
	// success
	STP_SUCCESS = 0,
	STP_ERROR_NONE = 0,

	// generic error
	STP_ERROR = 1,

	// device not connected
	STP_ERROR_DEVICE_NOT_CONNECTED = 2,

	// controller not connected
	STP_ERROR_CONTROLLER_NOT_CONNECTED = 3,

	// controller/device not synced
	STP_ERROR_NOT_SYNCED = 4,

	// device already open
	STP_ERROR_ALREADY_OPEN = 5,

	// device already closed
	STP_ERROR_ALREADY_CLOSED = 6,

	// device already closed
	STP_ERROR_INVALID_SESSION = 7,

	// invalid parameters
	STP_ERROR_INVALID_PARAMETERS = 8,

	// STP not initialized
	STP_ERROR_STP_NOT_INITIALIZED = 9,

	// invalid command
	STP_ERROR_INVALID_COMMAND = 10,

	// SoC suspended
	STP_ERROR_SOC_SUSPENDED = 11,

	STP_ERROR_IO_INTERRUPT = 12,

	// STP is not re-entrant from inside a callback on the main STP thread
	STP_ERROR_INSIDE_CALLBACK = 13,

	// Channel not available during initialization
	STP_ERROR_CHANNEL_UNAVAILABLE = 14,

	// Space unavailable for non-blocking writes
	STP_ERROR_TX_SPACE_UNAVAILABLE = 15,
	// Data unavailable for non-blocking reads (intentionally the same value as the space unavailable error)
	STP_ERROR_RX_DATA_UNAVAILABLE = 15,

	// STP next error
	STP_ERROR_NEXT_ERROR = 16,
};

/* get attributes */
enum stp_get_attribute_type {
	// Get available TX pipeline
	STP_TX_AVAILABLE = 0,
	STP_TX_DATA,
	// Get filled RX pipeline
	STP_RX_FILLED,
	// Get transaction stats
	STP_STATS,
	// Get sync status
	STP_ATTRIB_SYNCED,
	// Sync status
	STP_ATTRIB_VALID_SESSION,
	// Valid session status
	STP_ATTRIB_CONTROLLER_CONNECTED,
	// Get device connected status
	STP_ATTRIB_DEVICE_CONNECTED,
	STP_WAIT_FOR_DEVICE,
	STP_WAIT_FOR_DATA,
	// Channels status
	STP_ATTRIB_CHANNELS_STATUS,

	STP_ATTRIB_TX_PIPELINE_SIZE,
	STP_ATTRIB_RX_PIPELINE_SIZE,

	// If a device channel is inside a callback
	STP_ATTRIB_DEBUG_IN_CALLBACK,

	STP_NUM_GET_ATTRIBUTES
};

/* set attributes */
enum stp_set_attribute_type {
	// set rx_available_data_limit_notification
	STP_ATTRIB_RX_DATA_LIMIT_NOTIFICATION = 0,
	// set tx_available_space_limit_notification
	STP_ATTRIB_TX_SPACE_LIMIT_NOTIFICATION,

	STP_NUM_SET_ATTRIBUTES
};

/*  notifications */
enum stp_notification_type {
	// STP synced
	STP_NOTIFICATION_SYNCED = 0,
	// STP unsynced
	STP_NOTIFICATION_UNSYNCED,
	// CHANNEL AVAILABLE (open on both sides)
	STP_NOTIFICATION_CHANNEL_AVAILABLE,
	// CHANNEL NOT_AVAILABLE (not open on both sides)
	STP_NOTIFICATION_CHANNEL_NOT_AVAILABLE,
	// Data available in RX pipeline
	STP_NOTIFICATION_RX_DATA_AVAILABLE,
	// Space available in TX pipeline
	STP_NOTIFICATION_TX_SPACE_AVAILABLE,
};

/**
 * Structure containing the options for opening a channel
 */
struct stp_channel_opts_t {
	uint8_t priority;
	uint8_t *rx_buffer;
	size_t rx_buffer_size;
	uint8_t *tx_buffer;
	size_t tx_buffer_size;
};

typedef void (*stp_channel_callback)(uint8_t channel, uint32_t notification);

#ifdef __cplusplus
}
#endif

#endif

#ifndef STP_COMMON_PUBLIC_H
#define STP_COMMON_PUBLIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stp/common/stp_os.h>

// total number of channels
#define STP_TOTAL_NUM_CHANNELS 32

// maximum valid channel index (0 to STP_TOTAL_NUM_CHANNELS - 1)
#define STP_MAX_CHANNEL (STP_TOTAL_NUM_CHANNELS - 1)

// total number of channel priorities
#define STP_TOTAL_NUM_PRIORITIES 32

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
	STP_ERROR_RX_DATA_UNAVAILABLE = 16,

	// STP next error
	STP_ERROR_NEXT_ERROR = 17,

	// Interruption on service
	STP_ERROR_SERVICE_INTERRUPTION = 18,

	STP_ERROR_NUM_ERRORS,
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
	// Underlying transport service has interruption
	STP_ATTRIB_SERVICE_INTERRUPTION,
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

/* X-macro listing all smartglasses error codes
 * X(STP_NOTIFICATION)
*/
#define STP_NOTIFICATIONS                                                      \
	X(SYNCED)                                                              \
	X(UNSYNCED)                                                            \
	X(CHANNEL_AVAILABLE)                                                   \
	X(CHANNEL_NOT_AVAILABLE)                                               \
	X(RX_DATA_AVAILABLE)                                                   \
	X(TX_SPACE_AVAILABLE)

/*  notifications */
enum stp_notification_type {
#ifdef X
#undef X
#endif // X
#define X(notification) STP_NOTIFICATION_##notification,
	STP_NOTIFICATIONS
#undef X
};

/**
 * Get the string name of a notification
 * @param notification the notification type
 * @return the string name of the notification, "unknown" if not found
 */
const char *stp_get_notification_name(enum stp_notification_type notification);

/**
 * Check if a channel number is valid
 * @param channel the channel number to check
 * @return true if channel is valid (0 to STP_TOTAL_NUM_CHANNELS - 1), false otherwise
 */
static inline bool stp_channel_is_valid(uint8_t channel)
{
	return channel <= STP_MAX_CHANNEL;
}

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

#pragma pack(push, 1)
#pragma pack(1)
/** Clock synchronization data, allowing correlation of a remote and local clock. */
typedef struct {
	/** When, in the target clock domain, this packet was signaled to the receiver. */
	uint64_t sender_signal_ns;

	/** When, in the local clock domain, the signal from the sender was received. */
	uint64_t receiver_signal_ns;
} stp_clock_t;
#pragma pack(pop)

typedef void (*stp_channel_callback)(uint8_t channel, uint32_t notification);

#ifdef __cplusplus
}
#endif

#endif

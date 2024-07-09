/* SPDX-License-Identifier: GPL-2.0 */
#ifndef STP_CONTROLLER_H
#define STP_CONTROLLER_H

#include <stp/common/stp_os.h>
#include <stp/common/stp_common.h>
#include <stp/common/stp_common_public.h>

#ifdef __cplusplus
extern "C" {
#endif

/* get attributes */
enum stp_controller_callback_event_type {
	/* Init event */
	STP_CONTROLLER_EVENT_INIT = 0,

	STP_CONTROLLER_NUM_EVENTS
};

/**
 * Set attributes
 */
enum stp_controller_set_attribute_type {
	STP_CONTROLLER_ATTRIB_SET_LOG_RX_DATA = STP_NUM_GET_ATTRIBUTES,

	STP_CONTROLLER_ATTRIB_SET_LOG_TX_DATA
};

/* Transport callbacks. Defined by the controller/device clients*/
struct stp_controller_transport_table {
	/* callback for send/receive data */
	int32_t (*send_receive_data)(uint8_t *tx, uint8_t *rx, uint32_t len);
};

/* controller callbacks. Defined by the controller client */
struct stp_controller_handshake_table {
	/* check if device has data */
	bool (*device_has_data)(void);
	/* check if device can receive data */
	bool (*device_can_receive)(void);
};

struct stp_controller_wait_signal_table {
	int32_t (*wait_write)(uint8_t channel);
	void (*signal_write)(uint8_t channel);
	int32_t (*wait_read)(uint8_t channel);
	void (*signal_read)(uint8_t channel);
	int32_t (*wait_fsync)(uint8_t channel);
	void (*signal_fsync)(uint8_t channel);
	void (*reset_fsync)(uint8_t channel);
	int32_t (*wait_open)(uint8_t channel);
	void (*signal_open)(uint8_t channel);
	void (*signal_stp_event)(void);
	int32_t (*wait_stp_event)(void);
	int32_t (*pause_thread)(void);
	int32_t (*resume_thread)(void);
};

struct stp_controller_init_t {
	struct stp_controller_transport_table *transport;
	struct stp_controller_handshake_table *handshake;
	struct stp_controller_wait_signal_table *wait_signal;
	// Buffer used to interact with the spi send. Must be STP_TOTAL_DATA_SIZE
	uint8_t *rx_buffer;
	// Buffer used to interact with the spi send. Must be STP_TOTAL_DATA_SIZE
	uint8_t *tx_buffer;
};

/* controller/device write data in blocking mode*/
int32_t stp_controller_write(uint8_t channel, const uint8_t *buffer,
			     uint32_t buffer_size, uint32_t *data_size);

/* controller/device write data in non-blocking mode*/
int32_t stp_controller_write_nb(uint8_t channel, const uint8_t *buffer,
				uint32_t size, uint32_t *data_size);

/* controller/device read data in non-blocking mode data */
int32_t stp_controller_read_nb(uint8_t channel, uint8_t *buffer, uint32_t size,
			       uint32_t *data_size);

/* controller/device read data in blocking mode data */
int32_t stp_controller_read(uint8_t channel, uint8_t *buffer,
			    uint32_t buffer_size, uint32_t *data_size);

/* Read a minimum number of bytes, blocking if necessary */
int32_t stp_controller_read_minimum(uint8_t channel, uint8_t *buffer,
				    uint32_t buffer_size, uint32_t *data_size,
				    uint32_t minimum);

/* controller transaction function */
int32_t stp_controller_transaction_thread(void);

/* controller initialization */
int32_t stp_controller_init(struct stp_controller_init_t *init);

/* controller de-init */
int32_t stp_controller_deinit(void);

/* Get attributes */
int32_t stp_controller_get_attribute(uint32_t attribute, void *p);

/* Get channel attributes */
int32_t stp_controller_get_channel_attribute(uint8_t channel,
					     uint32_t attribute, void *p);

/* Set channel attributes */
int32_t stp_controller_set_channel_attribute32(uint8_t channel,
					       uint32_t attribute,
					       uint32_t value);

/* Set attributes */
int32_t stp_controller_set(uint32_t attribute, void *p);

int32_t stp_controller_set_callback(void (*callback)(int));

void stp_controller_stp_signal_start(void);

int32_t stp_controller_open(uint8_t channel, uint8_t priority,
			    uint8_t *rx_buffer, size_t rx_buffer_size,
			    uint8_t *tx_buffer, size_t tx_buffer_size);

int32_t stp_controller_open_blocking(uint8_t channel, uint8_t priority,
				     uint8_t *rx_buffer, size_t rx_buffer_size,
				     uint8_t *tx_buffer, size_t tx_buffer_size);

int32_t stp_controller_close(uint8_t channel);

int32_t stp_controller_fsync(uint8_t channel);

bool stp_controller_pending_event(void);

#ifdef __cplusplus
}
#endif

#endif

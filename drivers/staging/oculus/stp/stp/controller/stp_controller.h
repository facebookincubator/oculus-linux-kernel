/*
 * SPI STP Controller external header
 *
 * Copyright (C) 2020 Eugen Pirvu
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

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
	/* check if device request transaction */
	bool (*device_request_transaction)(void);
	/* Set controller has data */
	void (*set_controller_has_data)(bool value);

	/** Returns whether the controller is advertising that it has data. */
	bool (*get_controller_has_data)(void);

	/** Returns the last time the controller advertised that it had data. */
	uint64_t (*get_controller_last_has_data_ns)(void);

	/** Returns the current controller time. */
	uint64_t (*stp_controller_get_ns)(void);
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

	/** Channel used to carry timstamping info. */
	uint8_t time_channel;
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

void stp_controller_reset_all_channel_buffer(void);

void stp_controller_request_protocol_resync(void);

/* Log the controller state machine and handshake bookkeeping for debugging. */
void stp_dump_controller_state(const char *reason);

#ifdef __cplusplus
}
#endif

#endif
